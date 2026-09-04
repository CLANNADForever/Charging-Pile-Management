#!/usr/bin/env node
'use strict';

const fs=require('fs');
const path=require('path');
const os=require('os');
const crypto=require('crypto');
const cp=require('child_process');
const profileLib=require('./project_profile_lib.js');

const POLICY_CONTEXTS=['local','ci','release'];
const POLICY_KEYS=['allow_command_ids','deny_command_ids','allow_capabilities','max_timeout_seconds','inherit_env','allow_side_effects','output_limit_bytes'];
const SECRET=/(?:authorization|proxy-authorization|bearer|x-?api-?key|api[_-]?key|token|password|secret|cookie|client[_-]?secret|private[_-]?key|access[_-]?key)[\s:=]+\S+|:\/\/[^/\s:]+:[^/@\s]+@|-----BEGIN [A-Z ]*PRIVATE KEY-----/ig;
const SECRET_ENV=/(?:TOKEN|SECRET|PASSWORD|PASSWD|API_?KEY|PRIVATE_?KEY|CREDENTIAL|COOKIE|AUTHORIZATION)/i;
const INJECTION_ENV=new Set(['BASH_ENV','ENV','SHELLOPTS','BASHOPTS','LD_PRELOAD','LD_LIBRARY_PATH','NODE_OPTIONS','PYTHONPATH','PERL5OPT','RUBYOPT']);
const sha=value=>'sha256:'+crypto.createHash('sha256').update(value).digest('hex');
const entryExists=file=>{try{fs.lstatSync(file);return true}catch(error){if(error.code==='ENOENT')return false;throw error}};
const closed=(o,keys,label)=>{
  if(!o||typeof o!=='object'||Array.isArray(o)||Object.keys(o).some(k=>!keys.includes(k))||keys.some(k=>!Object.prototype.hasOwnProperty.call(o,k)))
    throw new Error(label+' schema mismatch');
};
const ids=(value,label)=>{
  if(!Array.isArray(value)||value.some(v=>typeof v!=='string'||!(v==='*'||/^[a-z][a-z0-9]*(?:-[a-z0-9]+)*$/.test(v)))||new Set(value).size!==value.length)
    throw new Error(label+' must be a unique identifier array');
  return value;
};
const envNames=(value,label)=>{
  if(!Array.isArray(value)||value.some(v=>typeof v!=='string'||!/^[A-Z_][A-Z0-9_]*$/.test(v)||
      SECRET_ENV.test(v)||INJECTION_ENV.has(v)||v==='LANG'||v==='LC_ALL'||v.startsWith('AUTOAI_'))||
     new Set(value).size!==value.length)
    throw new Error(label+' must be a unique environment-name array');
  return value;
};
const known=(value,label,allowed)=>{
  ids(value,label);
  if(value.some(item=>item!=='*'&&!allowed.has(item)))throw new Error(label+' contains an unknown value');
  return value;
};
const match=(value,patterns)=>patterns.includes('*')||patterns.includes(value);
const redact=value=>String(value).replace(SECRET,'<redacted>');
const truncate=(value,limit)=>{
  const buffer=Buffer.from(value);
  if(buffer.length<=limit)return {text:redact(buffer.toString('utf8')),truncated:false,bytes:buffer.length};
  return {text:redact(buffer.subarray(0,limit).toString('utf8'))+'\n<output-truncated>',truncated:true,bytes:buffer.length};
};

function loadPolicy(root,context,ciProfileId){
  const policyFile=path.join(root,'.ai-harness','organization-policy.json');
  let policy=null,policySha=null;
  if(entryExists(policyFile)){
    const entry=profileLib.safeRepositoryEntry(root,'.ai-harness/organization-policy.json','organization policy','file');
    policy=profileLib.parseJsonStrict(fs.readFileSync(entry.path,'utf8'),'organization policy');
    closed(policy,['schema_version','policy_id','contexts'],'organization policy');
    if(policy.schema_version!==1||typeof policy.policy_id!=='string'||!/^[a-z][a-z0-9]*(?:-[a-z0-9]+)*$/.test(policy.policy_id))
      throw new Error('organization policy identity mismatch');
    closed(policy.contexts,POLICY_CONTEXTS,'organization policy contexts');
    for(const name of POLICY_CONTEXTS){
      const item=policy.contexts[name];
      closed(item,POLICY_KEYS,'organization policy '+name);
      ids(item.allow_command_ids,'allow_command_ids');
      ids(item.deny_command_ids,'deny_command_ids');
      known(item.allow_capabilities,'allow_capabilities',profileLib.CAPABILITIES);
      envNames(item.inherit_env,'inherit_env');
      known(item.allow_side_effects,'allow_side_effects',profileLib.SIDE_EFFECTS);
      if(!Number.isInteger(item.max_timeout_seconds)||item.max_timeout_seconds<1||item.max_timeout_seconds>86400)
        throw new Error('organization policy timeout is invalid');
      if(!Number.isInteger(item.output_limit_bytes)||item.output_limit_bytes<1024||item.output_limit_bytes>1048576)
        throw new Error('organization policy output limit is invalid');
    }
    policySha=sha(Buffer.from(profileLib.canonical(policy)));
  }else if(context!=='local'){
    throw new Error(context+' execution requires an explicit organization policy');
  }

  let ciProfile=null,ciProfileSha=null;
  if(ciProfileId){
    if(context==='local')throw new Error('CI profile cannot be used in local context');
    if(!/^[a-z][a-z0-9]*(?:-[a-z0-9]+)*$/.test(ciProfileId))throw new Error('invalid CI profile ID');
    const relative='.ai-harness/ci-profiles/'+ciProfileId+'.json';
    const entry=profileLib.safeRepositoryEntry(root,relative,'CI profile','file');
    ciProfile=profileLib.parseJsonStrict(fs.readFileSync(entry.path,'utf8'),'CI profile');
    closed(ciProfile,['schema_version','profile_id','context','allow_command_ids','deny_command_ids','max_timeout_seconds','inherit_env'],'CI profile');
    if(ciProfile.schema_version!==1||ciProfile.profile_id!==ciProfileId||ciProfile.context!==context)
      throw new Error('CI profile identity mismatch');
    ids(ciProfile.allow_command_ids,'CI allow_command_ids');
    ids(ciProfile.deny_command_ids,'CI deny_command_ids');
    envNames(ciProfile.inherit_env,'CI inherit_env');
    if(!Number.isInteger(ciProfile.max_timeout_seconds)||ciProfile.max_timeout_seconds<1||ciProfile.max_timeout_seconds>86400)
      throw new Error('CI profile timeout is invalid');
    ciProfileSha=sha(Buffer.from(profileLib.canonical(ciProfile)));
  }
  return {policy,policy_sha256:policySha,ci_profile:ciProfile,ci_profile_sha256:ciProfileSha};
}

function authorize(command,policyState,context){
  let timeout=command.timeout_seconds,outputLimit=65536,env=[...command.inherit_env];
  if(policyState.policy){
    const rule=policyState.policy.contexts[context];
    if(match(command.id,rule.deny_command_ids)||!match(command.id,rule.allow_command_ids)||
       !match(command.capability,rule.allow_capabilities))
      throw new Error('organization policy '+policyState.policy.policy_id+' denies command '+command.id);
    for(const effect of command.side_effects)
      if(!match(effect,rule.allow_side_effects))throw new Error('organization policy '+policyState.policy.policy_id+' denies side effect '+effect);
    timeout=Math.min(timeout,rule.max_timeout_seconds);
    outputLimit=Math.min(outputLimit,rule.output_limit_bytes);
    env=env.filter(name=>rule.inherit_env.includes(name));
  }else for(const effect of command.side_effects)if(effect!=='workspace-write')throw new Error('side effect '+effect+' requires an explicit organization policy');
  if(policyState.ci_profile){
    const rule=policyState.ci_profile;
    if(match(command.id,rule.deny_command_ids)||!match(command.id,rule.allow_command_ids))
      throw new Error('CI profile '+rule.profile_id+' denies command '+command.id);
    timeout=Math.min(timeout,rule.max_timeout_seconds);
    env=env.filter(name=>rule.inherit_env.includes(name));
  }
  return {timeout_seconds:timeout,output_limit_bytes:outputLimit,inherit_env:env};
}

function buildEnvironment(names,extra={}){
  const env={};
  for(const name of names)if(Object.prototype.hasOwnProperty.call(process.env,name))env[name]=process.env[name];
  return {...env,LANG:'C.UTF-8',LC_ALL:'C.UTF-8',...extra};
}
function canonicalizePathEnvironment(env,cwd){
  if(!Object.prototype.hasOwnProperty.call(env,'PATH'))return env;
  return {...env,PATH:env.PATH.split(path.delimiter)
    .map(directory=>directory?path.resolve(cwd,directory):cwd)
    .join(path.delimiter)};
}

function runChild(command,cwd,env,timeoutSeconds,limit){
  return new Promise((resolve,reject)=>{
    const started=Date.now(),stdout=[],stderr=[];
    const stdoutHash=crypto.createHash('sha256'),stderrHash=crypto.createHash('sha256');
    let stdoutBytes=0,stderrBytes=0,timedOut=false,finished=false,cancelledSignal=null,killTimer=null;
    const child=cp.spawn(command.argv[0],command.argv.slice(1),{
      cwd,env,shell:false,detached:process.platform!=='win32',stdio:['ignore','pipe','pipe']
    });
    const signalExit={SIGHUP:129,SIGINT:130,SIGTERM:143};
    const collect=(chunks,key,chunk)=>{
      if(key==='stdout'){stdoutBytes+=chunk.length;stdoutHash.update(chunk)}
      else{stderrBytes+=chunk.length;stderrHash.update(chunk)}
      const used=chunks.reduce((n,x)=>n+x.length,0);
      if(used<limit)chunks.push(chunk.subarray(0,Math.max(0,limit-used)));
    };
    child.stdout.on('data',chunk=>collect(stdout,'stdout',chunk));
    child.stderr.on('data',chunk=>collect(stderr,'stderr',chunk));
    const terminate=signal=>{
      try{
        if(process.platform!=='win32'&&child.pid)process.kill(-child.pid,signal);
        else child.kill(signal);
      }catch(error){if(error.code!=='ESRCH')throw error}
    };
    const escalate=()=>{if(killTimer===null){killTimer=setTimeout(()=>{try{terminate('SIGKILL')}catch{}},1000);killTimer.unref()}};
    const cleanup=()=>{clearTimeout(timer);if(killTimer!==null)clearTimeout(killTimer);for(const signal of Object.keys(signalExit))process.removeListener(signal,handlers[signal]);process.removeListener('exit',onParentExit)};
    const handlers=Object.fromEntries(Object.keys(signalExit).map(signal=>[signal,()=>{if(finished||cancelledSignal)return;cancelledSignal=signal;terminate('SIGTERM');escalate()}]));
    const onParentExit=()=>{if(!finished)try{terminate('SIGKILL')}catch{}};
    for(const [signal,handler] of Object.entries(handlers))process.once(signal,handler);
    process.once('exit',onParentExit);
    child.on('error',error=>{if(!finished){finished=true;cleanup();reject(error)}});
    const timer=setTimeout(()=>{
      timedOut=true;
      terminate('SIGTERM');
      escalate();
    },timeoutSeconds*1000);
    child.on('close',(code,signal)=>{
      if(finished)return;
      finished=true;cleanup();
      const out=truncate(Buffer.concat(stdout),limit),err=truncate(Buffer.concat(stderr),limit);
      out.bytes=stdoutBytes;err.bytes=stderrBytes;
      const stdoutSha='sha256:'+stdoutHash.digest('hex'),stderrSha='sha256:'+stderrHash.digest('hex');
      resolve({exit_code:cancelledSignal?signalExit[cancelledSignal]:timedOut?124:Number.isInteger(code)?code:1,signal:cancelledSignal||signal||null,timed_out:timedOut,cancelled:cancelledSignal!==null,started,finished:Date.now(),stdout:out,stderr:err,stdout_sha256:stdoutSha,stderr_sha256:stderrSha,output_sha256:sha(Buffer.from(stdoutSha+'\0'+stderrSha))});
    });
  });
}

function sourceFingerprint(root){
  const value=cp.execFileSync(path.join(root,'scripts','source_fingerprint.sh'),['--kind','source'],{
    cwd:root,encoding:'utf8',env:{...process.env,GIT_EXTERNAL_DIFF:'',GIT_DIFF_OPTS:''}
  }).trim();
  if(!/^sha256:[0-9a-f]{64}$/.test(value))throw new Error('source fingerprint is invalid');
  return value;
}
function changeFingerprint(root,kind,change){
  const value=cp.execFileSync(path.join(root,'scripts','source_fingerprint.sh'),['--kind',kind,'--change',change],{
    cwd:root,encoding:'utf8',env:{...process.env,GIT_EXTERNAL_DIFF:'',GIT_DIFF_OPTS:''}
  }).trim();
  if(!/^sha256:[0-9a-f]{64}$/.test(value))throw new Error(kind+' fingerprint is invalid');
  return value;
}
function resolveExecutable(root,cwd,value,env,label){
  let candidate;
  if(value.startsWith('./'))candidate=path.join(root,...value.slice(2).split('/'));
  else{
    for(const directory of (env.PATH||'').split(path.delimiter)){
      const base=directory?path.resolve(cwd,directory):cwd,probe=path.join(base,value);
      try{
        const target=fs.statSync(probe);
        if(target.isFile()&&(target.mode&0o111)!==0){candidate=probe;break}
      }catch{}
    }
    if(!candidate)throw new Error(label+' is unavailable on the effective PATH: '+value);
  }
  const link=fs.lstatSync(candidate);
  if(value.startsWith('./')&&link.isSymbolicLink())throw new Error(label+' is not a safe repository executable: '+value);
  const real=fs.realpathSync(candidate),target=fs.statSync(real,{bigint:true});
  if(!target.isFile()||(target.mode&0o111n)===0n)throw new Error(label+' is not an executable regular file: '+value);
  if(value.startsWith('./')&&real!==root&&!real.startsWith(root+path.sep))
    throw new Error(label+' resolves outside repository: '+value);
  return real;
}
function executableIdentity(file,role,name){
  const noFollow=fs.constants.O_NOFOLLOW||0,fd=fs.openSync(file,fs.constants.O_RDONLY|noFollow);
  try{
    const before=fs.fstatSync(fd,{bigint:true}),hash=crypto.createHash('sha256'),buffer=Buffer.allocUnsafe(65536);
    if(!before.isFile()||(before.mode&0o111n)===0n)throw new Error('resolved executable is no longer executable: '+name);
    for(;;){const count=fs.readSync(fd,buffer,0,buffer.length,null);if(count===0)break;hash.update(buffer.subarray(0,count))}
    const after=fs.fstatSync(fd,{bigint:true});
    for(const key of ['dev','ino','size','mode','mtimeNs','ctimeNs'])if(before[key]!==after[key])
      throw new Error('resolved executable changed while its identity was read: '+name);
    return {
      role,name,
      realpath_sha256:sha(Buffer.from(file)),
      content_sha256:'sha256:'+hash.digest('hex'),
      size:Number(before.size),
      mode:Number(before.mode&0o777n)
    };
  }finally{fs.closeSync(fd)}
}
function executableInventory(root,command,env){
  const cwd=path.join(root,...command.cwd.split('/'));
  const executable=resolveExecutable(root,cwd,command.argv[0],env,'project command '+command.id);
  const rows=[executableIdentity(executable,'argv0',command.argv[0])];
  for(const name of command.required_tools){
    const required=resolveExecutable(root,cwd,name,env,'required project tool');
    rows.push(executableIdentity(required,'required-tool',name));
  }
  rows.sort((a,b)=>Buffer.from(a.role+'\0'+a.name).compare(Buffer.from(b.role+'\0'+b.name)));
  return {
    sha256:sha(Buffer.from(profileLib.canonical(rows))),
    command:{...command,argv:[executable,...command.argv.slice(1)]}
  };
}

async function main(){
  const args=process.argv.slice(2);
  if(!args.length)throw new Error('usage: project_command.sh <command-id> [--change <id>] [--json]');
  const commandId=args.shift();
  if(!/^[a-z][a-z0-9]*(?:-[a-z0-9]+)*$/.test(commandId))throw new Error('invalid command ID');
  let change=null,asJson=false,identityOnly=false;
  while(args.length){
    const arg=args.shift();
    if(arg==='--json')asJson=true;
    else if(arg==='--internal-runtime-identity'){
      if(identityOnly)throw new Error('duplicate internal runtime identity option');
      identityOnly=true;
    }
    else if(arg==='--change'){
      change=args.shift();
      if(!change||!/^[a-z][a-z0-9]*(?:-[a-z0-9]+)*$/.test(change)||change==='archive'||change.startsWith('archive-')||change==='stale'||change.startsWith('stale-'))
        throw new Error('invalid change ID');
    }else throw new Error('unknown project command option: '+arg);
  }
  const root=profileLib.ensureRepositoryRoot(process.cwd());
  if(change){
    profileLib.safeRepositoryEntry(root,'openspec/changes/'+change,'active change','directory');
    const selectorEntry=profileLib.safeRepositoryEntry(root,'ai_snapshot.json','active selector','file');
    const selector=profileLib.parseJsonStrict(fs.readFileSync(selectorEntry.path,'utf8'),'active selector');
    if(selector.schema_version!==2||selector.workflow!=='openspec'||selector.active_change!==change)
      throw new Error('requested change is not the active change');
  }
  const loaded=profileLib.parseProfile(root,path.join(root,'.ai-harness','project-profile.json'));
  const command=loaded.commands.get(commandId);
  if(!command)throw new Error('unknown Project Profile command: '+commandId);
  const context=process.env.AUTOAI_EXECUTION_CONTEXT||'local';
  if(!POLICY_CONTEXTS.includes(context))throw new Error('invalid execution context');
  const policy=loadPolicy(root,context,process.env.AUTOAI_CI_PROFILE||null);
  const effective=authorize(command,policy,context);
  if(!effective.inherit_env.includes('PATH')&&(!command.argv[0].startsWith('./')||command.required_tools.length))
    throw new Error('PATH command or required_tools need PATH in the effective environment allowlist');
  const cwd=path.join(root,...command.cwd.split('/'));
  const env=canonicalizePathEnvironment(buildEnvironment(effective.inherit_env,{
    AUTOAI_PROJECT_COMMAND_ID:command.id,
    AUTOAI_PROJECT_MODULES:command.module_ids.join(','),
    AUTOAI_EXECUTION_CONTEXT:context,
    ...(change?{AUTOAI_ACTIVE_CHANGE:change}:{})
  }),cwd);
  const commandExecutableBefore=executableInventory(root,command,env);
  const environmentSha=sha(Buffer.from(profileLib.canonical(
    Object.fromEntries(Object.keys(env).sort().map(name=>[name,sha(Buffer.from(env[name]))]))
  )));
  const sourceBefore=sourceFingerprint(root);
  const artifactBefore=change?changeFingerprint(root,'artifact',change):null;
  const planningBefore=change?changeFingerprint(root,'planning',change):null;

  const identities=[];
  for(const identity of loaded.profile.toolchain_identity){
    if(!identity.module_ids.some(id=>command.module_ids.includes(id)))continue;
    const probe=loaded.commands.get(identity.command_id),probeEffective=authorize(probe,policy,context);
    const probeCwd=path.join(root,...probe.cwd.split('/'));
    const probeEnv=canonicalizePathEnvironment(
      buildEnvironment(probeEffective.inherit_env,{AUTOAI_EXECUTION_CONTEXT:context}),
      probeCwd
    );
    if(!probeEffective.inherit_env.includes('PATH')&&(!probe.argv[0].startsWith('./')||probe.required_tools.length))
      throw new Error('toolchain identity command lost its required PATH');
    const probeExecutableBefore=executableInventory(root,probe,probeEnv);
    const result=await runChild(probeExecutableBefore.command,probeCwd,probeEnv,probeEffective.timeout_seconds,16384);
    if(result.exit_code!==0||result.timed_out)throw new Error('toolchain identity probe failed: '+identity.id);
    const probeExecutableAfter=executableInventory(root,probe,probeEnv);
    if(probeExecutableBefore.sha256!==probeExecutableAfter.sha256)
      throw new Error('toolchain identity executable inventory changed during probe: '+identity.id);
    identities.push({
      id:identity.id,
      command_id:probe.id,
      executable_inventory_sha256:probeExecutableBefore.sha256,
      output_sha256:result.output_sha256
    });
  }
  const toolchainSha=sha(Buffer.from(profileLib.canonical(identities)));
  const commandModules=command.module_ids.map(id=>loaded.modules.get(id));
  const adapterSha=sha(Buffer.from(profileLib.canonical({
    contract_version:profileLib.ADAPTER_CONTRACT_VERSION,
    modules:commandModules.map(module=>({id:module.id,adapter:module.adapter}))
  })));
  const targetSha=sha(Buffer.from(profileLib.canonical(commandModules.map(module=>({
    id:module.id,cpp_standards:module.cpp_standards,
    compilers:module.compilers,target_platforms:module.target_platforms
  })))));
  const platformSha=sha(Buffer.from(profileLib.canonical({
    platform:process.platform,arch:process.arch,kernel:os.release()
  })));
  if(identityOnly){
    const sourceAfterProbe=sourceFingerprint(root);
    const artifactAfterProbe=change?changeFingerprint(root,'artifact',change):null;
    const planningAfterProbe=change?changeFingerprint(root,'planning',change):null;
    const loadedAfterProbe=profileLib.parseProfile(root,path.join(root,'.ai-harness','project-profile.json'));
    const policyAfterProbe=loadPolicy(root,context,process.env.AUTOAI_CI_PROFILE||null);
    const commandExecutableAfterProbe=executableInventory(root,command,env);
    if(sourceBefore!==sourceAfterProbe||artifactBefore!==artifactAfterProbe||planningBefore!==planningAfterProbe||
       loaded.profile_sha256!==loadedAfterProbe.profile_sha256||policy.policy_sha256!==policyAfterProbe.policy_sha256||
       policy.ci_profile_sha256!==policyAfterProbe.ci_profile_sha256||
       commandExecutableBefore.sha256!==commandExecutableAfterProbe.sha256)
      throw new Error('runtime identity probe changed project, planning, Profile or policy state');
    process.stdout.write(JSON.stringify({
      schema_version:1,
      command_id:command.id,
      execution_context:context,
      ci_profile_id:process.env.AUTOAI_CI_PROFILE||null,
      toolchain_identity_sha256:toolchainSha,
      platform_identity_sha256:platformSha,
      allowed_environment_sha256:environmentSha,
      executable_inventory_sha256:commandExecutableBefore.sha256
    },null,2)+'\n');
    return;
  }
  const startedAt=new Date().toISOString();
  const result=await runChild(commandExecutableBefore.command,cwd,env,effective.timeout_seconds,effective.output_limit_bytes);
  const finishedAt=new Date().toISOString();
  const sourceAfter=sourceFingerprint(root);
  const artifactAfter=change?changeFingerprint(root,'artifact',change):null;
  const planningAfter=change?changeFingerprint(root,'planning',change):null;
  const loadedAfter=profileLib.parseProfile(root,path.join(root,'.ai-harness','project-profile.json'));
  const policyAfter=loadPolicy(root,context,process.env.AUTOAI_CI_PROFILE||null);
  const commandExecutableAfter=executableInventory(root,command,env);
  const stable=sourceBefore===sourceAfter&&artifactBefore===artifactAfter&&planningBefore===planningAfter&&
    loaded.profile_sha256===loadedAfter.profile_sha256&&policy.policy_sha256===policyAfter.policy_sha256&&
    policy.ci_profile_sha256===policyAfter.ci_profile_sha256&&
    commandExecutableBefore.sha256===commandExecutableAfter.sha256;
  const identity={
    schema_version:1,
    profile_sha256:loaded.profile_sha256,
    profile_sha256_after:loadedAfter.profile_sha256,
    command_id:command.id,
    command_identity_sha256:profileLib.commandIdentity(loaded,command),
    module_ids:command.module_ids,
    capability:command.capability,
    canonical_argv:command.argv,
    canonical_cwd:command.cwd,
    execution_context:context,
    policy_sha256:policy.policy_sha256,
    policy_sha256_after:policyAfter.policy_sha256,
    ci_profile_id:process.env.AUTOAI_CI_PROFILE||null,
    ci_profile_sha256:policy.ci_profile_sha256,
    ci_profile_sha256_after:policyAfter.ci_profile_sha256,
    adapter_identity_sha256:adapterSha,
    toolchain_identity_sha256:toolchainSha,
    target_identity_sha256:targetSha,
    platform_identity_sha256:platformSha,
    allowed_environment_sha256:environmentSha,
    executable_inventory_sha256:commandExecutableBefore.sha256,
    change_name:change,
    source_fingerprint_before:sourceBefore,
    source_fingerprint_after:sourceAfter,
    artifact_fingerprint_before:artifactBefore,
    artifact_fingerprint_after:artifactAfter,
    planning_fingerprint_before:planningBefore,
    planning_fingerprint_after:planningAfter
  };
  const envelope={
    schema_version:1,
    status:result.cancelled?'Cancelled':result.timed_out?'TimedOut':!stable?'Stale':result.exit_code===0?'Pass':'Fail',
    started_at:startedAt,finished_at:finishedAt,
    duration_ms:Math.max(0,result.finished-result.started),
    exit_code:result.exit_code,signal:result.signal,
    identity,
    stdout:result.stdout.text,stderr:result.stderr.text,
    stdout_bytes:result.stdout.bytes,stderr_bytes:result.stderr.bytes,
    stdout_sha256:result.stdout_sha256,stderr_sha256:result.stderr_sha256,
    output_truncated:result.stdout.truncated||result.stderr.truncated,
    output_sha256:result.output_sha256
  };
  envelope.evidence_subject_sha256=sha(Buffer.from(profileLib.canonical({
    identity,status:envelope.status,exit_code:envelope.exit_code,output_sha256:envelope.output_sha256
  })));
  if(asJson)process.stdout.write(JSON.stringify(envelope,null,2)+'\n');
  else{
    if(envelope.stdout)process.stdout.write(envelope.stdout+(envelope.stdout.endsWith('\n')?'':'\n'));
    if(envelope.stderr)process.stderr.write(envelope.stderr+(envelope.stderr.endsWith('\n')?'':'\n'));
    process.stderr.write(`[AUTOAI] project-command=${command.id} subject=${envelope.evidence_subject_sha256} profile=${loaded.profile_sha256}\n`);
  }
  process.exitCode=envelope.status==='Stale'?6:result.exit_code;
}

main().catch(error=>{
  console.error('[ERR] '+redact(error.message));
  process.exitCode=4;
});
