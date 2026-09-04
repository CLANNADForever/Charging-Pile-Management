#!/usr/bin/env node
'use strict';
const fs=require('fs'),path=require('path'),cp=require('child_process');
const lib=require('./project_profile_lib.js');
const args=new Set(process.argv.slice(2)),json=args.has('--json'),strict=args.has('--strict');
if([...args].some(x=>!['--json','--strict'].includes(x))){console.error('usage: harness_doctor.sh [--json] [--strict]');process.exit(2)}
const checks=[];
const add=(id,category,status,severity,message,suggestion=null,affected_capabilities=[])=>checks.push({id,category,status,severity,message,suggestion,affected_capabilities});
const check=(id,category,fn,severity='error',suggestion=null,affected=[])=>{
  try{const message=fn();add(id,category,'pass',severity,message||'ok',null,affected);return true}
  catch(error){add(id,category,'fail',severity,error.message,suggestion,affected);return false}
};
let root=process.cwd(),loaded=null,activeChange=null;
const entryExists=file=>{try{fs.lstatSync(file);return true}catch(error){if(error.code==='ENOENT')return false;throw error}};
const pathDirectories=(process.env.PATH||'').split(path.delimiter);
const resolvePathCommand=(name,cwd=root)=>{
  for(const directory of pathDirectories){const base=directory?path.resolve(cwd,directory):cwd,candidate=path.join(base,name);try{const st=fs.lstatSync(candidate);if(!st.isFile()&&!st.isSymbolicLink())continue;const real=fs.realpathSync(candidate),target=fs.statSync(real);if(target.isFile()&&(target.mode&0o111)!==0)return real}catch{}}
  throw Error(name+' executable was not found on PATH');
};
check('control.git.root','control',()=>{
  const top=cp.execFileSync('git',['rev-parse','--show-toplevel'],{encoding:'utf8'}).trim();
  if(fs.realpathSync(top)!==fs.realpathSync(root))throw Error('run from Git repository root');
  return 'Git repository root is valid';
});
check('control.node.version','control',()=>{
  const p=process.versions.node.split('.').map(Number),m=[20,19,0];
  for(let i=0;i<3;i++){if(p[i]>m[i])break;if(p[i]<m[i])throw Error('Node >=20.19.0 required')}
  return 'Node '+process.versions.node;
});
check('control.npm.available','control',()=>resolvePathCommand('npm'));
check('control.npx.available','control',()=>resolvePathCommand('npx'));
check('control.openspec.wrapper','control',()=>{
  const p=path.join(root,'scripts','openspec_cli.sh'),s=fs.lstatSync(p),text=fs.readFileSync(p,'utf8');
  if(!s.isFile()||s.isSymbolicLink()||(s.mode&0o111)===0||!text.includes('@fission-ai/openspec@1.6.0')||!text.includes('OPENSPEC_TELEMETRY=0'))throw Error('pinned OpenSpec wrapper is invalid');
  return 'pinned wrapper present; Doctor did not execute npx';
});
check('control.manifest','control',()=>{require('./manifest_policy.js').loadManifest(root);return 'managed manifest and template contents are valid'});
check('control.workflow-contract','control',()=>{cp.execFileSync(path.join(root,'scripts','workflow_contract_check.sh'),[],{cwd:root,stdio:'pipe'});return 'workflow contract parity passed'});
check('control.profile.schema','profile',()=>{
  loaded=lib.parseProfile(root,path.join(root,'.ai-harness','project-profile.json'));
  return loaded.profile_sha256;
});
check('workflow.active.selector','workflow',()=>{
  const file=path.join(root,'ai_snapshot.json'),s=fs.lstatSync(file),d=JSON.parse(fs.readFileSync(file,'utf8'));
  const validChange=value=>typeof value==='string'&&/^[a-z][a-z0-9]*(?:-[a-z0-9]+)*$/.test(value)&&value!=='archive'&&!value.startsWith('archive-')&&value!=='stale'&&!value.startsWith('stale-');
  if(!s.isFile()||s.isSymbolicLink()||d.schema_version!==2||d.workflow!=='openspec'||!(d.active_change===null||validChange(d.active_change)))throw Error('active selector is invalid');
  if(d.active_change!==null){
    const active=path.join(root,'openspec','changes',d.active_change),activeStat=fs.lstatSync(active);
    if(!activeStat.isDirectory()||activeStat.isSymbolicLink())throw Error('active selector points to a missing or unsafe change');
    const localFile=path.join(active,'harness','ai_snapshot.json'),localStat=fs.lstatSync(localFile),local=JSON.parse(fs.readFileSync(localFile,'utf8'));
    if(!localStat.isFile()||localStat.isSymbolicLink()||![2,3,4].includes(local.schema_version)||Object.prototype.hasOwnProperty.call(local,'active_change'))throw Error('active change local snapshot is invalid');
  }
  activeChange=d.active_change;
  return d.active_change?'active change: '+d.active_change:'no active change';
});
check('workflow.evidence.freshness','workflow',()=>{
  if(activeChange===null)return 'no active change evidence to inspect';
  const h=path.join(root,'openspec','changes',activeChange,'harness'),snapshotFile=path.join(h,'ai_snapshot.json'),snapshot=JSON.parse(fs.readFileSync(snapshotFile,'utf8'));
  const manifest=require('./manifest_policy.js'),all=JSON.parse(cp.execFileSync(path.join(root,'scripts','source_fingerprint.sh'),['--kind','all','--change',activeChange,'--json'],{cwd:root,encoding:'utf8'}));
  if(snapshot.planned_base_specs_fingerprint&&snapshot.planned_base_specs_fingerprint!==all.base_specs_fingerprint)throw Error('planned base specs fingerprint is stale');
  if(snapshot.schema_version>=3){
    const planning=manifest.planningState(root,activeChange);
    if(snapshot.planned_change_fingerprint&&snapshot.planned_change_fingerprint!==planning.planning_fingerprint||snapshot.planned_tdd_policy_sha256&&snapshot.planned_tdd_policy_sha256!==planning.tdd_policy_sha256||snapshot.schema_version===4&&snapshot.planned_integration_completeness_sha256&&snapshot.planned_integration_completeness_sha256!==planning.integration_completeness_sha256)throw Error('approved planning evidence is stale');
  }
  const baselineFile=path.join(h,'evaluation-baseline.json');if(!fs.existsSync(baselineFile))return 'planning evidence is current; no Evaluation baseline';
  const bs=fs.lstatSync(baselineFile),baseline=JSON.parse(fs.readFileSync(baselineFile,'utf8'));if(!bs.isFile()||bs.isSymbolicLink())throw Error('Evaluation baseline is unsafe');
  if(baseline.status==='in_progress')return 'planning evidence is current; Evaluation is in progress';
  if(baseline.status!=='complete')return 'planning evidence is current; latest Evaluation is not complete';
  const digest=file=>'sha256:'+require('crypto').createHash('sha256').update(fs.readFileSync(file)).digest('hex');
  if(baseline.source_fingerprint!==all.source_fingerprint||baseline.artifact_fingerprint!==all.artifact_fingerprint||baseline.base_specs_fingerprint!==all.base_specs_fingerprint||baseline.verification_json_sha256!==digest(path.join(h,'verification.json'))||baseline.evaluation_json_sha256!==digest(path.join(h,'evaluation.json')))throw Error('completed Evaluation evidence is stale');
  if(snapshot.schema_version===4)require('./integration_surface_lib.js').validateCompleteEvaluationV3({root,change:activeChange,change_root:path.join(root,'openspec','changes',activeChange)});
  else if(snapshot.schema_version===3)manifest.verifyTddEvidence(root,activeChange,{requireDone:true,sourceFingerprint:all.source_fingerprint});
  return 'planning and completed Evaluation evidence are current';
},'warning','Refresh planning or run a new independent Evaluation before archive.');
check('workflow.lock.state','workflow',()=>{
  const lock=path.join(root,'.ai-harness','locks','managed-operation.lock');
  if(!fs.existsSync(lock))return 'no managed operation lock';
  const s=fs.lstatSync(lock);if(!s.isDirectory()||s.isSymbolicLink())throw Error('unsafe managed lock');
  const pid=Number(fs.readFileSync(path.join(lock,'pid'),'utf8').trim());
  if(!Number.isInteger(pid)||pid<=1)throw Error('malformed managed lock');
  try{process.kill(pid,0)}catch(error){if(error.code!=='EPERM')throw Error('stale managed lock owner')}
  return 'managed lock owner is alive';
});
if(loaded){
  const commandAvailability=new Map;
  const resolve=command=>{
    if(command.argv[0].startsWith('./')){
      const p=path.join(root,...command.argv[0].slice(2).split('/'));
      const s=fs.lstatSync(p);if(!s.isFile()||s.isSymbolicLink()||(s.mode&0o111)===0)throw Error('repository command is missing or not executable');
      const real=fs.realpathSync(p);if(real!==root&&!real.startsWith(root+path.sep))throw Error('repository command resolves outside the repository');return real;
    }
    if(!command.inherit_env.includes('PATH'))throw Error('PATH command is not allowed to inherit PATH');
    return resolvePathCommand(command.argv[0],path.join(root,...command.cwd.split('/')));
  };
  for(const command of [...loaded.commands.values()].sort((a,b)=>Buffer.from(a.id).compare(Buffer.from(b.id)))){
    const cwdOk=check('profile.command.'+command.id+'.cwd','project-capability',()=>{
      const p=path.join(root,...command.cwd.split('/')),s=fs.lstatSync(p);if(!s.isDirectory()||s.isSymbolicLink())throw Error('command cwd is unavailable');return command.cwd
    },'error',null,[command.capability]);
    const executableOk=check('profile.command.'+command.id+'.executable','project-capability',()=>resolve(command),
      'warning','Install the project tool or update the reviewed Project Profile.',[command.capability]);
    const requiredToolsOk=check('profile.command.'+command.id+'.required-tools','project-capability',()=>{
      const cwd=path.join(root,...command.cwd.split('/'));
      for(const tool of command.required_tools)resolvePathCommand(tool,cwd);
      return command.required_tools.length?'required tools: '+command.required_tools.join(', '):'no additional required tools'
    },'warning','Install the required project tools or update the reviewed Project Profile.',[command.capability]);
    commandAvailability.set(command.id,cwdOk&&executableOk&&requiredToolsOk);
  }
  loaded.command_availability=commandAvailability;
}
if(entryExists(path.join(root,'.ai-harness','organization-policy.json'))){
  const policyOk=check('policy.organization.schema','policy',()=>{cp.execFileSync(path.join(root,'scripts','organization_policy.sh'),['--check'],{cwd:root,stdio:'pipe'});return 'organization policy is valid'});
  if(policyOk&&loaded){
    const organizationEntry=lib.safeRepositoryEntry(root,'.ai-harness/organization-policy.json','organization policy','file');
    const organization=lib.parseJsonStrict(fs.readFileSync(organizationEntry.path,'utf8'),'organization policy'),rule=organization.contexts.local,allows=(value,patterns)=>patterns.includes('*')||patterns.includes(value);
    for(const command of loaded.commands.values()){
      const allowed=check('profile.command.'+command.id+'.local-policy','policy',()=>{
        if(allows(command.id,rule.deny_command_ids)||!allows(command.id,rule.allow_command_ids)||!allows(command.capability,rule.allow_capabilities)||command.side_effects.some(effect=>!allows(effect,rule.allow_side_effects)))throw Error('organization policy '+organization.policy_id+' denies this local capability');
        const effectiveEnv=command.inherit_env.filter(name=>rule.inherit_env.includes(name));
        if((!command.argv[0].startsWith('./')||command.required_tools.length)&&!effectiveEnv.includes('PATH'))throw Error('organization policy '+organization.policy_id+' removes PATH required by this command');
        return 'allowed by organization policy '+organization.policy_id;
      },'warning','Update the policy or use a permitted command.',[command.capability]);
      loaded.command_availability.set(command.id,loaded.command_availability.get(command.id)&&allowed);
    }
  }
}else if(loaded){
  for(const command of loaded.commands.values())if(command.side_effects.some(effect=>effect!=='workspace-write')){
    const allowed=check('profile.command.'+command.id+'.default-policy','policy',()=>{throw Error('sensitive side effects require an explicit organization policy')},'warning','Add a reviewed organization policy for network, install, device or remote writes.',[command.capability]);
    loaded.command_availability.set(command.id,loaded.command_availability.get(command.id)&&allowed);
  }
}
const ciDirectory=path.join(root,'.ai-harness','ci-profiles');
if(entryExists(ciDirectory))check('policy.ci-profiles.schema','policy',()=>{
  const directory=lib.safeRepositoryEntry(root,'.ai-harness/ci-profiles','CI profile directory','directory').path;
  const closed=(o,keys,label)=>{if(!o||typeof o!=='object'||Array.isArray(o)||Object.keys(o).some(k=>!keys.includes(k))||keys.some(k=>!Object.prototype.hasOwnProperty.call(o,k)))throw Error(label+' schema mismatch')};
  const secretEnv=/(?:TOKEN|SECRET|PASSWORD|PASSWD|API_?KEY|PRIVATE_?KEY|CREDENTIAL|COOKIE|AUTHORIZATION)/i,injectionEnv=new Set(['BASH_ENV','ENV','SHELLOPTS','BASHOPTS','LD_PRELOAD','LD_LIBRARY_PATH','NODE_OPTIONS','PYTHONPATH','PERL5OPT','RUBYOPT']);
  const commandPattern=value=>value==='*'||/^[a-z][a-z0-9]*(?:-[a-z0-9]+)*$/.test(value);
  const envName=value=>/^[A-Z_][A-Z0-9_]*$/.test(value)&&!secretEnv.test(value)&&!injectionEnv.has(value)&&value!=='LANG'&&value!=='LC_ALL'&&!value.startsWith('AUTOAI_');
  for(const name of fs.readdirSync(directory).sort()){
    if(!/^[a-z][a-z0-9]*(?:-[a-z0-9]+)*\.json$/.test(name))throw Error('unexpected CI profile entry: '+name);
    const relative='.ai-harness/ci-profiles/'+name,entry=lib.safeRepositoryEntry(root,relative,'CI profile '+name,'file');
    const d=lib.parseJsonStrict(fs.readFileSync(entry.path,'utf8'),'CI profile '+name);
    closed(d,['schema_version','profile_id','context','allow_command_ids','deny_command_ids','max_timeout_seconds','inherit_env'],'CI profile');
    if(d.schema_version!==1||d.profile_id!==name.slice(0,-5)||!['ci','release'].includes(d.context)||!Number.isInteger(d.max_timeout_seconds)||d.max_timeout_seconds<1||d.max_timeout_seconds>86400)throw Error('CI profile identity mismatch: '+name);
    for(const key of ['allow_command_ids','deny_command_ids'])if(!Array.isArray(d[key])||d[key].length!==new Set(d[key]).size||d[key].some(value=>typeof value!=='string'||!commandPattern(value)))throw Error('CI profile array mismatch: '+name+'/'+key);
    if(!Array.isArray(d.inherit_env)||d.inherit_env.length!==new Set(d.inherit_env).size||d.inherit_env.some(value=>typeof value!=='string'||!envName(value)))throw Error('CI profile array mismatch: '+name+'/inherit_env');
  }
  return 'all CI profiles are valid';
});
const runtimeCommandAvailability=new Map;
if(loaded){
  for(const command of [...loaded.commands.values()].sort((a,b)=>Buffer.from(a.id).compare(Buffer.from(b.id)))){
    const identityCommandIds=[...new Set(loaded.profile.toolchain_identity
      .filter(identity=>identity.module_ids.some(id=>command.module_ids.includes(id)))
      .map(identity=>identity.command_id))].sort((a,b)=>Buffer.from(a).compare(Buffer.from(b)));
    const identityReady=check('profile.command.'+command.id+'.toolchain-identities','project-capability',()=>{
      const unavailable=identityCommandIds.filter(id=>!loaded.command_availability.get(id));
      if(unavailable.length)throw Error('required toolchain identity commands are unavailable: '+unavailable.join(', '));
      return identityCommandIds.length?'runtime identities: '+identityCommandIds.join(', '):'no runtime toolchain identity dependency';
    },'warning','Restore the identity probe tools or update the reviewed Project Profile.',[command.capability]);
    runtimeCommandAvailability.set(command.id,loaded.command_availability.get(command.id)&&identityReady);
  }
}
const failed=checks.filter(x=>x.status==='fail'&&(x.severity==='error'||strict&&x.severity==='warning'));
const errors=checks.filter(x=>x.status==='fail'&&x.severity==='error').length,warnings=checks.filter(x=>x.status==='fail'&&x.severity==='warning').length;
const byteSort=(left,right)=>Buffer.from(left).compare(Buffer.from(right));
const moduleCapabilities=loaded?[...loaded.modules.values()]
  .sort((left,right)=>byteSort(left.id,right.id))
  .flatMap(module=>[...lib.CAPABILITIES].sort(byteSort).map(capability=>{
    const commandIds=[...(module.capabilities[capability]||[])].sort(byteSort);
    const declared=module.capability_status[capability]||null;
    const declaredStatus=declared?.status||(commandIds.length?'available':'absent');
    const availableCommandIds=commandIds.filter(id=>runtimeCommandAvailability.get(id));
    const unavailableCommandIds=commandIds.filter(id=>!runtimeCommandAvailability.get(id));
    const status=commandIds.length
      ? unavailableCommandIds.length===0?'available':availableCommandIds.length===0?'unavailable':'partially_available'
      : declaredStatus;
    return {
      module_id:module.id,
      capability,
      status,
      declared_status:declaredStatus,
      reason:declared?.reason||(commandIds.length?'available command binding declared in the reviewed Project Profile':'not declared in the reviewed Project Profile'),
      command_ids:commandIds,
      available_command_ids:availableCommandIds,
      unavailable_command_ids:unavailableCommandIds
    };
  })):[];
const declaredCapabilities=[...lib.CAPABILITIES].sort(byteSort);
const capabilityStatus=new Map(declaredCapabilities.map(capability=>{
  const statuses=moduleCapabilities.filter(row=>row.capability===capability).map(row=>row.status);
  const available=statuses.filter(value=>value==='available').length;
  const status=statuses.length&&available===statuses.length?'available':
    available||statuses.includes('partially_available')?'partially_available':
    statuses.includes('unavailable')?'unavailable':
    statuses.includes('needs-approval')?'needs-approval':
    statuses.length&&statuses.every(value=>value==='not-applicable')?'not-applicable':'absent';
  return [capability,status];
}));
const availableCapabilities=declaredCapabilities.filter(capability=>capabilityStatus.get(capability)==='available');
const unavailableCapabilities=declaredCapabilities.filter(capability=>capabilityStatus.get(capability)==='unavailable');
const partiallyAvailableCapabilities=declaredCapabilities.filter(capability=>capabilityStatus.get(capability)==='partially_available');
const absentCapabilities=declaredCapabilities.filter(capability=>capabilityStatus.get(capability)==='absent');
const notApplicableCapabilities=declaredCapabilities.filter(capability=>capabilityStatus.get(capability)==='not-applicable');
const needsApprovalCapabilities=declaredCapabilities.filter(capability=>capabilityStatus.get(capability)==='needs-approval');
const summary={
  status:errors||strict&&warnings?'fail':warnings?'degraded':'pass',
  errors,
  warnings,
  available_capabilities:availableCapabilities,
  unavailable_capabilities:unavailableCapabilities,
  partially_available_capabilities:partiallyAvailableCapabilities,
  absent_capabilities:absentCapabilities,
  not_applicable_capabilities:notApplicableCapabilities,
  needs_approval_capabilities:needsApprovalCapabilities,
  module_capabilities:moduleCapabilities
};
if(json)console.log(JSON.stringify({schema_version:1,checks,summary},null,2));
else{for(const item of checks)console.log(`[${item.status==='pass'?'OK':item.severity==='error'?'ERR':'WARN'}] ${item.id}: ${item.message}`);console.log(`Doctor: ${summary.status} (${summary.errors} errors, ${summary.warnings} warnings)`)}
if(failed.length)process.exit(6);
