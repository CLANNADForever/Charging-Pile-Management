#!/usr/bin/env node
'use strict';
const fs=require('fs'),path=require('path'),crypto=require('crypto'),cp=require('child_process');

const templatePaths=[
  'PROJECT_ATTRIBUTION.md','CLAUDE.md','AGENTS.md','init.sh','.cursorrules',
  '.claude/settings.json','.claude/skills/full-code-review/SKILL.md',
  '.codex/skills/full-code-review/SKILL.md','.codex/skills/full-code-review/agents/openai.yaml',
  'docs/ai/openspec.md','docs/ai/implementation-economy.md','docs/ai/workflow.md',
  'docs/ai/evaluation.md','docs/ai/check-rules.md','docs/ai/quick-brief.md',
  'docs/ai/tooling.md','docs/ai/cpp.md','docs/ai/testing.md','docs/ai/build.md',
  'docs/ai/golden-principles.md','docs/ai/rca.md',
  'prompts/planner.md','prompts/generator.md','prompts/evaluator.md','prompts/archive.md',
  'prompts/handoff.md','prompts/resume.md','prompts/full-code-review.md','prompts/rca.md',
  'prompts/debt-scan.md','prompts/debt-fix.md',
  'scripts/openspec_cli.sh','scripts/openspec_preflight.sh','scripts/attribution_check.sh',
  'scripts/harness_lock.sh','scripts/manifest_policy.js',
  'scripts/project_detect.sh','scripts/project_profile_lib.js','scripts/project_profile.sh',
  'scripts/project_command.js','scripts/project_command.sh',
  'scripts/workflow_contract_check.js','scripts/workflow_contract_check.sh',
  'scripts/harness_doctor.js','scripts/harness_doctor.sh',
  'scripts/project_index.js','scripts/project_index.sh','scripts/context_slice.js','scripts/context_slice.sh',
  'scripts/campaign.js','scripts/campaign.sh','scripts/event_audit.js','scripts/event_audit.sh',
  'scripts/organization_policy.js','scripts/organization_policy.sh',
  'scripts/change_scope.js','scripts/integration_surface_lib.js','scripts/integration_surface_check.sh',
  'scripts/clang_ast_surface_adapter.js','scripts/change_new.sh','scripts/change_adopt.sh',
  'scripts/change_select.sh','scripts/change_status.sh','scripts/change_archive.sh',
  'scripts/archive_recover.sh','scripts/source_fingerprint.sh','scripts/change_footprint.sh',
  'scripts/task_verify.sh','scripts/evaluator_check.sh','scripts/verification_workspace.sh',
  'scripts/snapshot_update.sh','scripts/resume_from_snapshot.sh','scripts/context_reset_check.sh',
  'scripts/quick_brief_check.sh','scripts/rca_new.sh','scripts/ai_debt_scan.sh',
  '.ai-harness/workflow-contract.json'
];
const v2Only=new Set([
  'scripts/project_detect.sh','scripts/project_profile_lib.js','scripts/project_profile.sh',
  'scripts/project_command.js','scripts/project_command.sh',
  'scripts/workflow_contract_check.js','scripts/workflow_contract_check.sh',
  'scripts/harness_doctor.js','scripts/harness_doctor.sh',
  'scripts/project_index.js','scripts/project_index.sh','scripts/context_slice.js','scripts/context_slice.sh',
  'scripts/campaign.js','scripts/campaign.sh','scripts/event_audit.js','scripts/event_audit.sh',
  'scripts/organization_policy.js','scripts/organization_policy.sh',
  '.ai-harness/workflow-contract.json','.ai-harness/project-profile.json',
  '.ai-harness/organization-policy.json','.ai-harness/campaigns/','.ai-harness/ci-profiles/'
]);
const expected=new Map([
  ['.ai-harness/manifest.json',['metadata',null]],['.gitignore',['template-append',null]],
  ...templatePaths.map(p=>[p,['template',2]]),
  ['.ai-harness/project-profile.json',['team',null]],
  ['.ai-harness/organization-policy.json',['team',null]],
  ['.ai-harness/campaigns/',['team-prefix',null]],['.ai-harness/ci-profiles/',['team-prefix',null]],
  ['ai_snapshot.json',['runtime',null]],['claude-progress.txt',['runtime',null]],
  ['session-state.md',['runtime',null]],['debt-register.md',['runtime',null]],
  ['defect-rca.md',['runtime',null]],['openspec/config.yaml',['team',null]],
  ['openspec/specs/',['team-prefix',null]],['openspec/changes/',['team-prefix',null]]
]);
const legacyExpected=new Map([
  ['.ai-harness/manifest.json',['metadata',1]],['.gitignore',['template-append',1]],
  ...templatePaths.filter(p=>!v2Only.has(p)).map(p=>[p,['template',1]]),
  ['.clang-format',['template',1]],['.vscode/settings.json',['template',1]],
  ['.vscode/extensions.json',['template',1]],
  ['ai_snapshot.json',['runtime',null]],['claude-progress.txt',['runtime',null]],
  ['session-state.md',['runtime',null]],['debt-register.md',['runtime',null]],
  ['defect-rca.md',['runtime',null]],['openspec/config.yaml',['team',null]],
  ['openspec/specs/',['team-prefix',null]],['openspec/changes/',['team-prefix',null]]
]);
// Historical manifests may omit earlier reviewed template-family additions.
// A v1 -> v2 upgrade also introduces the universal-project paths above.
const templateUpgradePaths=new Set([
  ...v2Only,'PROJECT_ATTRIBUTION.md','scripts/attribution_check.sh',
  'scripts/change_scope.js','scripts/integration_surface_lib.js',
  'scripts/integration_surface_check.sh','scripts/clang_ast_surface_adapter.js',
  'scripts/verification_workspace.sh'
]);
const legacyExcludes=['.ai-harness/locks','.ai-harness/logs','.ai-harness/migrations'];
const expectedExcludes=[...legacyExcludes,'.ai-harness/derived'];
const closed=(o,keys,name)=>{if(!o||typeof o!=='object'||Array.isArray(o)||Object.keys(o).some(k=>!keys.includes(k))||keys.some(k=>!(k in o)))throw Error(name+' schema mismatch')};
const safe=p=>{if(typeof p!=='string'||!p||path.posix.isAbsolute(p)||p.includes('\\')||p.includes('\0')||p.includes('\n')||path.posix.normalize(p).startsWith('../'))throw Error('unsafe manifest path: '+String(p));return p};
function loadManifest(root=process.cwd(),options={}){
  const file=path.join(root,'.ai-harness/manifest.json'),st=fs.lstatSync(file);if(!st.isFile()||st.isSymbolicLink())throw Error('manifest must be a regular file');
  const d=JSON.parse(fs.readFileSync(file,'utf8')),legacy=d?.schema_version===1,current=d?.schema_version===2;
  if(legacy)closed(d,['schema_version','workflow_mode','openspec','last_template_update_at','managed_paths','local_excludes'],'manifest v1');
  else if(current)closed(d,['schema_version','harness_version','workflow_mode','openspec','last_template_update_at','managed_paths','local_excludes'],'manifest v2');
  else throw Error('unsupported manifest schema');
  closed(d.openspec,['package','version','schema'],'manifest.openspec');
  if(current&&d.harness_version!=='4.0.0'||d.workflow_mode!=='openspec'||d.openspec.package!=='@fission-ai/openspec'||d.openspec.version!=='1.6.0'||d.openspec.schema!=='spec-driven'||!Number.isFinite(Date.parse(d.last_template_update_at))||!Array.isArray(d.managed_paths))throw Error('manifest identity mismatch');
  const contract=legacy?legacyExpected:expected,entries=new Map;
  for(const x of d.managed_paths){
    closed(x,legacy?['path','ownership','template_version']:['path','ownership','template_version','content_sha256'],'manifest entry');
    safe(x.path);if(entries.has(x.path))throw Error('duplicate manifest path: '+x.path);
    const want=contract.get(x.path);if(!want||x.ownership!==want[0]||x.template_version!==want[1])throw Error('unapproved manifest ownership: '+x.path);
    if(current){
      const shouldHash=x.ownership==='template';
      if(shouldHash?!/^sha256:[0-9a-f]{64}$/.test(x.content_sha256||''):x.content_sha256!==null)throw Error('manifest content digest shape mismatch: '+x.path);
      if(shouldHash&&options.verifyContent!==false){
        const target=path.join(root,...x.path.split('/')),targetStat=fs.lstatSync(target);
        if(!targetStat.isFile()||targetStat.isSymbolicLink()||x.content_sha256!=='sha256:'+crypto.createHash('sha256').update(fs.readFileSync(target)).digest('hex'))throw Error('managed template content mismatch: '+x.path);
      }
    }
    entries.set(x.path,x);
  }
  const contractMissing=[...contract.keys()].filter(p=>!entries.has(p));
  if(entries.size+contractMissing.length!==contract.size||contractMissing.some(p=>!options.allowTemplateUpgrade||!templateUpgradePaths.has(p)))throw Error('manifest managed path set is incomplete');
  const excludes=legacy?legacyExcludes:expectedExcludes;
  if(!Array.isArray(d.local_excludes)||d.local_excludes.length!==excludes.length||d.local_excludes.some((p,i)=>p!==excludes[i]))throw Error('manifest local_excludes mismatch');
  const missing=[...expected.keys()].filter(p=>!entries.has(p));
  const upgradePaths=legacy?[...new Set([...missing,'.ai-harness/manifest.json'])].sort(cmp):missing.sort(cmp);
  const isManaged=p=>{safe(p);if(p==='.ai-harness/archive-transaction.json')return true;for(const [name,x] of entries){if(x.ownership==='template-append')continue;if(name.endsWith('/')){if(p.startsWith(name))return true}else if(p===name||x.ownership==='template'&&p.startsWith(name+'.bak.'))return true}return expectedExcludes.some(x=>p===x||p.startsWith(x+'/'))};
  return {manifest:d,isManaged,upgrade_required:upgradePaths.length>0,missing_managed_paths:upgradePaths};
}
const digest=b=>'sha256:'+crypto.createHash('sha256').update(b).digest('hex');
const cmp=(a,b)=>Buffer.from(a).compare(Buffer.from(b));
const canonical=v=>Array.isArray(v)?'['+v.map(canonical).join(',')+']':v&&typeof v==='object'?'{'+Object.keys(v).sort(cmp).map(k=>JSON.stringify(k)+':'+canonical(v[k])).join(',')+'}':JSON.stringify(v);
const safePattern=(value,label)=>{if(typeof value!=='string'||!value||path.posix.isAbsolute(value)||value.includes('\\')||value.includes('\0')||value.includes('\n')||value.includes('\r')||value.startsWith('!')||value.startsWith(':'))throw Error(label+' is not a safe repository path pattern');const probe=value.replace(/[?*]+/g,'x'),normal=path.posix.normalize(probe);if(normal==='..'||normal.startsWith('../')||normal==='.git'||normal.startsWith('.git/'))throw Error(label+' escapes or targets Git metadata');return value};
const globPattern=value=>{safePattern(value,'glob pattern');let s='^';for(let i=0;i<value.length;i++){const c=value[i];if(c==='*'&&value[i+1]==='*'){const segmentStart=i===0||value[i-1]==='/';if(segmentStart&&value[i+2]==='/'){s+='(?:.*/)?';i+=2}else if(segmentStart&&i+2===value.length){s+='.*';i++}else{s+='[^/]*';i++}}else if(c==='*')s+='[^/]*';else if(c==='?')s+='[^/]';else s+='\\.^$+{}()|[]'.includes(c)?'\\'+c:c}return new RegExp(s+'$')};
const matchesPattern=(value,patterns)=>{safePattern(value,'repository path');if(value.includes('*')||value.includes('?'))throw Error('repository path cannot be a glob');return patterns.some(p=>globPattern(p).test(value))};
function parseTaskContracts(tasksText){
  if(typeof tasksText!=='string')throw Error('tasks text required');const lines=tasksText.split(/\r?\n/),tasks=new Map;
  for(let i=0;i<lines.length;i++){const m=lines[i].match(/^- \[([ xX])\] (\d+(?:\.\d+)*)\s+(.+)$/);if(!m)continue;if(tasks.has(m[2]))throw Error('duplicate task ID: '+m[2]);const verify=[];for(let j=i+1;j<lines.length&&!/^- \[[ xX]\] \d/.test(lines[j]);j++){const v=lines[j].match(/^\s+- Verify:\s*(.+)$/);if(v)for(const x of v[1].matchAll(/`(build|test|behavior|static)`/g))verify.push(x[1])}if(!verify.length||verify.length!==new Set(verify).size)throw Error('task Verify contract missing or duplicated: '+m[2]);tasks.set(m[2],{id:m[2],done:m[1]!==' ',verify})}
  if(!tasks.size)throw Error('at least one leaf task required');return tasks;
}
function parseTddPolicy(designText,tasksText){
  if(typeof designText!=='string')throw Error('design text required');const start='<!-- autoai:tdd-policy:v1 -->',end='<!-- /autoai:tdd-policy:v1 -->';if(designText.split(start).length!==2||designText.split(end).length!==2)throw Error('exactly one TDD Policy v1 block required');const body=designText.slice(designText.indexOf(start)+start.length,designText.indexOf(end)),match=body.match(/^\s*```json\s*\n([\s\S]*?)\n```\s*$/);if(!match)throw Error('TDD Policy v1 must contain one JSON fence');const policy=JSON.parse(match[1]),closed=(o,keys,label)=>{if(!o||typeof o!=='object'||Array.isArray(o)||Object.keys(o).some(k=>!keys.includes(k))||keys.some(k=>!(k in o)))throw Error(label+' schema mismatch')},nonempty=(v,label)=>{if(typeof v!=='string'||!v.trim()||/[\r\n]/.test(v))throw Error(label+' must be non-empty and single-line')};closed(policy,['schema_version','default','exceptions'],'TDD policy');if(policy.schema_version!==1||policy.default!=='required'||!Array.isArray(policy.exceptions))throw Error('invalid TDD policy header');const tasks=parseTaskContracts(tasksText),ids=new Set,covered=new Set,categories=new Set(['generated_output','documentation_only','configuration_only','disposable_prototype','unavailable_hardware','unavailable_external_service']);
  for(const [i,x]of policy.exceptions.entries()){closed(x,['id','category','task_ids','paths','reason','alternative_verify_kinds','exit_condition'],`TDD exception ${i}`);if(typeof x.id!=='string'||!/^[a-z][a-z0-9]*(?:-[a-z0-9]+)*$/.test(x.id)||ids.has(x.id))throw Error('invalid or duplicate TDD exception ID');ids.add(x.id);if(!categories.has(x.category))throw Error('invalid TDD exception category: '+x.id);if(!Array.isArray(x.task_ids)||!x.task_ids.length||x.task_ids.some(id=>!tasks.has(id)||covered.has(id)))throw Error('unknown or overlapping TDD exception task');x.task_ids.forEach(id=>covered.add(id));if(!Array.isArray(x.paths)||!x.paths.length)throw Error('TDD exception paths required: '+x.id);for(const [j,p]of x.paths.entries())safePattern(p,`TDD exception ${x.id} path ${j}`);nonempty(x.reason,'TDD exception reason');nonempty(x.exit_condition,'TDD exception exit condition');if(!Array.isArray(x.alternative_verify_kinds)||!x.alternative_verify_kinds.length||x.alternative_verify_kinds.some(v=>!['build','test','behavior','static'].includes(v))||x.alternative_verify_kinds.length!==new Set(x.alternative_verify_kinds).size)throw Error('invalid alternative Verify kinds: '+x.id);for(const id of x.task_ids){const expected=[...tasks.get(id).verify].sort(),actual=[...x.alternative_verify_kinds].sort();if(JSON.stringify(expected)!==JSON.stringify(actual))throw Error('alternative Verify kinds must exactly match task '+id)}}
  return {policy,tasks,policy_sha256:digest(Buffer.from(canonical(policy)))};
}
function safeChangeRoot(root,change,requested){
  if(typeof change!=='string'||!/^[a-z][a-z0-9]*(?:-[a-z0-9]+)*$/.test(change))throw Error('invalid change ID');
  root=fs.realpathSync(path.resolve(root));const changes=path.join(root,'openspec','changes'),active=path.join(changes,change),archive=path.join(changes,'archive'),base=path.resolve(requested),escaped=change.replace(/[.*+?^${}()|[\]\\]/g,'\\$&'),archived=path.dirname(base)===archive&&new RegExp('^\\d{4}-\\d{2}-\\d{2}-'+escaped+'$').test(path.basename(base));
  if(base!==active&&!archived)throw Error('change root escaped active/archive roots');
  for(const dir of [path.join(root,'openspec'),changes,...(archived?[archive]:[]),base]){const st=fs.lstatSync(dir);if(!st.isDirectory()||st.isSymbolicLink())throw Error('unsafe change root ancestry')}
  return {root,base};
}
function planningStateAt(root,change,requested){
  const safeRoot=safeChangeRoot(root,change,requested);root=safeRoot.root;const base=safeRoot.base,required=['.openspec.yaml','proposal.md','design.md','tasks.md'],files=[];const add=rel=>{const file=path.join(base,...rel.split('/')),st=fs.lstatSync(file);if(!st.isFile()||st.isSymbolicLink())throw Error('unsafe planning artifact: '+rel);let content=fs.readFileSync(file);if(rel==='tasks.md')content=Buffer.from(content.toString('utf8').replace(/^- \[[xX ]\] (\d+(?:\.\d+)*)/gm,'- [ ] $1'));files.push({path:rel,mode:(st.mode&0o777).toString(8),sha256:digest(content)})};required.forEach(add);const specs=path.join(base,'specs'),sst=fs.lstatSync(specs);if(!sst.isDirectory()||sst.isSymbolicLink())throw Error('unsafe planning specs directory');const walk=(dir,rel)=>{for(const name of fs.readdirSync(dir).sort(cmp)){const p=path.join(dir,name),r=rel+'/'+name,st=fs.lstatSync(p);if(st.isSymbolicLink())throw Error('planning artifact symlink: '+r);if(st.isDirectory())walk(p,r);else if(st.isFile()&&name.endsWith('.md'))add(r);else throw Error('unexpected planning artifact: '+r)}};walk(specs,'specs');if(!files.some(x=>x.path.startsWith('specs/')))throw Error('no delta specs');files.sort((a,b)=>cmp(a.path,b.path));const design=fs.readFileSync(path.join(base,'design.md'),'utf8'),tasks=fs.readFileSync(path.join(base,'tasks.md'),'utf8'),parsed=parseTddPolicy(design,tasks),start='<!-- autoai:integration-completeness:v1 -->',end='<!-- /autoai:integration-completeness:v1 -->',hasStart=design.includes(start),hasEnd=design.includes(end);if(hasStart!==hasEnd)throw Error('partial Integration Completeness marker');const integration=hasStart?require(path.join(root,'scripts','integration_surface_lib.js')).parsePlanFromChangeRoot(root,change,base):null;return {planning_fingerprint:digest(Buffer.from(canonical(files))),tdd_policy_sha256:parsed.policy_sha256,integration_completeness_sha256:integration?.block_sha256||null,tdd_policy:parsed.policy,integration_completeness:integration?.block||null,tasks:parsed.tasks};
}
function planningState(root,change){
  root=path.resolve(root);return planningStateAt(root,change,path.join(root,'openspec','changes',change));
}
function projectCommandInvocation(argv,change){
  if(!Array.isArray(argv)||!['scripts/project_command.sh','./scripts/project_command.sh'].includes(argv[0]))return null;
  if(argv.length!==5||typeof argv[1]!=='string'||!/^[a-z][a-z0-9]*(?:-[a-z0-9]+)*$/.test(argv[1])||argv[2]!=='--change'||argv[3]!==change||argv[4]!=='--json')throw Error('Project Command evidence argv is not the managed closed form');
  return argv[1];
}
function currentJsonDigest(root,relative,label){
  const file=path.join(root,...relative.split('/'));try{fs.lstatSync(file)}catch(error){if(error.code==='ENOENT')return null;throw error}
  const profileLib=require(path.join(root,'scripts','project_profile_lib.js'));
  const entry=profileLib.safeRepositoryEntry(root,relative,label,'file');
  return digest(Buffer.from(canonical(profileLib.parseJsonStrict(fs.readFileSync(entry.path,'utf8'),label))));
}
function currentProjectCommandRuntimeIdentity(root,change,loaded,command,identity){
  const env={...process.env,AUTOAI_EXECUTION_CONTEXT:identity.execution_context};
  if(identity.ci_profile_id===null)delete env.AUTOAI_CI_PROFILE;
  else env.AUTOAI_CI_PROFILE=identity.ci_profile_id;
  const helper=path.join(root,'scripts','project_command.js');
  const raw=cp.execFileSync(process.execPath,[helper,command.id,'--change',change,'--internal-runtime-identity'],{
    cwd:root,encoding:'utf8',env,maxBuffer:1024*1024
  });
  let runtime;try{runtime=JSON.parse(raw)}catch{throw Error('Project Command runtime identity helper returned invalid JSON')}
  closed(runtime,['schema_version','command_id','execution_context','ci_profile_id','toolchain_identity_sha256','platform_identity_sha256','allowed_environment_sha256','executable_inventory_sha256'],'Project Command runtime identity');
  if(runtime.schema_version!==1||runtime.command_id!==command.id||runtime.execution_context!==identity.execution_context||runtime.ci_profile_id!==identity.ci_profile_id||![runtime.toolchain_identity_sha256,runtime.platform_identity_sha256,runtime.allowed_environment_sha256,runtime.executable_inventory_sha256].every(v=>/^sha256:[0-9a-f]{64}$/.test(v)))throw Error('Project Command runtime identity helper returned a mismatched identity');
  return runtime;
}
function validateProjectCommandEnvelope(root,change,command,raw,options={}){
  const commandId=projectCommandInvocation(command.argv,change);if(commandId===null)return null;
  if(!Buffer.isBuffer(raw))raw=Buffer.from(raw);
  if(!/^sha256:[0-9a-f]{64}$/.test(command.output_sha256)||digest(raw)!==command.output_sha256)throw Error('Project Command evidence output digest mismatch');
  let envelope;try{envelope=JSON.parse(raw)}catch{throw Error('Project Command evidence is not JSON')}
  if(!raw.equals(Buffer.from(JSON.stringify(envelope,null,2)+'\n')))throw Error('Project Command evidence bytes are not canonical');
  const own=(o,k)=>Object.prototype.hasOwnProperty.call(o,k),closed=(o,keys,label)=>{if(!o||typeof o!=='object'||Array.isArray(o)||Object.keys(o).some(k=>!keys.includes(k))||keys.some(k=>!own(o,k)))throw Error(label+' schema mismatch')};
  const envelopeKeys=['schema_version','status','started_at','finished_at','duration_ms','exit_code','signal','identity','stdout','stderr','stdout_bytes','stderr_bytes','stdout_sha256','stderr_sha256','output_truncated','output_sha256','evidence_subject_sha256'];
  const identityKeys=['schema_version','profile_sha256','profile_sha256_after','command_id','command_identity_sha256','module_ids','capability','canonical_argv','canonical_cwd','execution_context','policy_sha256','policy_sha256_after','ci_profile_id','ci_profile_sha256','ci_profile_sha256_after','adapter_identity_sha256','toolchain_identity_sha256','target_identity_sha256','platform_identity_sha256','allowed_environment_sha256','executable_inventory_sha256','change_name','source_fingerprint_before','source_fingerprint_after','artifact_fingerprint_before','artifact_fingerprint_after','planning_fingerprint_before','planning_fingerprint_after'];
  closed(envelope,envelopeKeys,'Project Command envelope');closed(envelope.identity,identityKeys,'Project Command identity');
  const identity=envelope.identity,isDigest=v=>typeof v==='string'&&/^sha256:[0-9a-f]{64}$/.test(v),nullableDigest=v=>v===null||isDigest(v),date=v=>{const n=Date.parse(v);if(!Number.isFinite(n)||n>Date.now()+300000)return null;return n};
  const started=date(envelope.started_at),finished=date(envelope.finished_at);
  if(envelope.schema_version!==1||identity.schema_version!==1||!['Pass','Fail','TimedOut','Cancelled'].includes(envelope.status)||started===null||finished===null||finished<started||!Number.isInteger(envelope.duration_ms)||envelope.duration_ms<0||!Number.isInteger(envelope.exit_code)||!(envelope.signal===null||typeof envelope.signal==='string')||typeof envelope.stdout!=='string'||typeof envelope.stderr!=='string'||!Number.isInteger(envelope.stdout_bytes)||envelope.stdout_bytes<0||!Number.isInteger(envelope.stderr_bytes)||envelope.stderr_bytes<0||typeof envelope.output_truncated!=='boolean'||![envelope.stdout_sha256,envelope.stderr_sha256,envelope.output_sha256,envelope.evidence_subject_sha256].every(isDigest))throw Error('Project Command envelope identity is invalid');
  if(envelope.output_sha256!==digest(Buffer.from(envelope.stdout_sha256+'\0'+envelope.stderr_sha256)))throw Error('Project Command stream digest relationship is invalid');
  if(envelope.status==='Pass'&&envelope.exit_code!==0||envelope.status==='Fail'&&envelope.exit_code===0||envelope.status==='TimedOut'&&envelope.exit_code!==124||envelope.status==='Cancelled'&&(!['SIGHUP','SIGINT','SIGTERM'].includes(envelope.signal)||![129,130,143].includes(envelope.exit_code))||command.exit_code!==envelope.exit_code)throw Error('Project Command status or exit code mismatch');
  if(typeof command.started_at==='string'&&date(command.started_at)>started||typeof command.finished_at==='string'&&date(command.finished_at)<finished)throw Error('Project Command envelope escaped its managed command interval');
  if(identity.change_name!==change||identity.command_id!==commandId||!['local','ci','release'].includes(identity.execution_context)||!Array.isArray(identity.module_ids)||identity.module_ids.length===0||identity.module_ids.some(x=>typeof x!=='string')||typeof identity.capability!=='string'||!Array.isArray(identity.canonical_argv)||identity.canonical_argv.some(x=>typeof x!=='string')||typeof identity.canonical_cwd!=='string'||![identity.profile_sha256,identity.profile_sha256_after,identity.command_identity_sha256,identity.adapter_identity_sha256,identity.toolchain_identity_sha256,identity.target_identity_sha256,identity.platform_identity_sha256,identity.allowed_environment_sha256,identity.executable_inventory_sha256,identity.source_fingerprint_before,identity.source_fingerprint_after,identity.artifact_fingerprint_before,identity.artifact_fingerprint_after,identity.planning_fingerprint_before,identity.planning_fingerprint_after].every(isDigest)||!nullableDigest(identity.policy_sha256)||!nullableDigest(identity.policy_sha256_after)||!nullableDigest(identity.ci_profile_sha256)||!nullableDigest(identity.ci_profile_sha256_after))throw Error('Project Command execution identity is invalid');
  if(identity.profile_sha256!==identity.profile_sha256_after||identity.policy_sha256!==identity.policy_sha256_after||identity.ci_profile_sha256!==identity.ci_profile_sha256_after||identity.source_fingerprint_before!==identity.source_fingerprint_after||identity.artifact_fingerprint_before!==identity.artifact_fingerprint_after||identity.planning_fingerprint_before!==identity.planning_fingerprint_after)throw Error('Project Command became stale during execution');
  const profileLib=require(path.join(root,'scripts','project_profile_lib.js')),loaded=profileLib.parseProfile(root,path.join(root,'.ai-harness','project-profile.json')),profileCommand=loaded.commands.get(commandId);
  const commandModules=profileCommand?profileCommand.module_ids.map(id=>loaded.modules.get(id)):[];
  const adapterIdentity=digest(Buffer.from(canonical({contract_version:profileLib.ADAPTER_CONTRACT_VERSION,modules:commandModules.map(module=>({id:module.id,adapter:module.adapter}))}))),targetIdentity=digest(Buffer.from(canonical(commandModules.map(module=>({id:module.id,cpp_standards:module.cpp_standards,compilers:module.compilers,target_platforms:module.target_platforms})))));
  if(!profileCommand||identity.profile_sha256!==loaded.profile_sha256||identity.command_identity_sha256!==profileLib.commandIdentity(loaded,profileCommand)||identity.adapter_identity_sha256!==adapterIdentity||identity.target_identity_sha256!==targetIdentity||canonical(identity.module_ids)!==canonical(profileCommand.module_ids)||identity.capability!==profileCommand.capability||canonical(identity.canonical_argv)!==canonical(profileCommand.argv)||identity.canonical_cwd!==profileCommand.cwd)throw Error('Project Command no longer matches the reviewed Project Profile');
  const policyDigest=currentJsonDigest(root,'.ai-harness/organization-policy.json','organization policy');
  if(identity.policy_sha256!==policyDigest)throw Error('Project Command organization policy is stale');
  if(identity.ci_profile_id===null){if(identity.ci_profile_sha256!==null)throw Error('Project Command CI profile identity mismatch')}
  else{
    if(typeof identity.ci_profile_id!=='string'||!/^[a-z][a-z0-9]*(?:-[a-z0-9]+)*$/.test(identity.ci_profile_id))throw Error('Project Command CI profile ID is invalid');
    const ciDigest=currentJsonDigest(root,'.ai-harness/ci-profiles/'+identity.ci_profile_id+'.json','CI profile');
    if(identity.ci_profile_sha256===null||identity.ci_profile_sha256!==ciDigest)throw Error('Project Command CI profile is stale');
  }
  if(options.revalidateRuntimeIdentity!==false){
    const runtime=currentProjectCommandRuntimeIdentity(root,change,loaded,profileCommand,identity);
    const stale=[];
    if(identity.toolchain_identity_sha256!==runtime.toolchain_identity_sha256)stale.push('toolchain');
    if(identity.platform_identity_sha256!==runtime.platform_identity_sha256)stale.push('platform');
    if(identity.allowed_environment_sha256!==runtime.allowed_environment_sha256)stale.push('allowed environment');
    if(identity.executable_inventory_sha256!==runtime.executable_inventory_sha256)stale.push('executable inventory');
    if(stale.length)throw Error('Project Command runtime environment or toolchain identity is stale: '+stale.join(', '));
  }
  if(options.source_fingerprint&&identity.source_fingerprint_before!==options.source_fingerprint||options.artifact_fingerprint&&identity.artifact_fingerprint_before!==options.artifact_fingerprint||options.planning_fingerprint&&identity.planning_fingerprint_before!==options.planning_fingerprint)throw Error('Project Command fingerprints do not match the enclosing evidence');
  for(const key of ['source_fingerprint','planning_fingerprint'])for(const side of ['before','after'])if(command[key+'_'+side]&&identity[key+'_'+side]!==command[key+'_'+side])throw Error('Project Command '+key+' does not match its task evidence');
  const expectedSubject=digest(Buffer.from(canonical({identity,status:envelope.status,exit_code:envelope.exit_code,output_sha256:envelope.output_sha256})));
  if(envelope.evidence_subject_sha256!==expectedSubject)throw Error('Project Command evidence subject digest mismatch');
  return {command_id:commandId,envelope};
}
function validateProjectCommandEvidence(root,change,command,options={}){
  const commandId=projectCommandInvocation(command.argv,change);if(commandId===null)return null;
  root=path.resolve(root);const requested=options.changeRoot||path.join(root,'openspec','changes',change),base=safeChangeRoot(root,change,requested).base,hex=command.output_sha256.slice(7),file=path.join(base,'harness','project-command-evidence',hex+'.json');
  let st;try{st=fs.lstatSync(file)}catch(e){if(e.code==='ENOENT')throw Error('Project Command evidence sidecar is missing: '+commandId);throw e}
  if(!st.isFile()||st.isSymbolicLink())throw Error('Project Command evidence sidecar is unsafe: '+commandId);
  const validated=validateProjectCommandEnvelope(root,change,command,fs.readFileSync(file),options);
  if(['TimedOut','Cancelled'].includes(validated.envelope.status)||validated.envelope.signal!==null)throw Error('Project Command terminal or signalled execution cannot close verification: '+commandId);
  return {...validated,path:path.relative(root,file).split(path.sep).join('/')};
}
function persistProjectCommandEvidence(root,change,commandId,exitCode,outputSha,outputFile){
  root=path.resolve(root);const safeRoot=safeChangeRoot(root,change,path.join(root,'openspec','changes',change)),raw=fs.readFileSync(outputFile),envelope=JSON.parse(raw),command={argv:['scripts/project_command.sh',commandId,'--change',change,'--json'],output_sha256:outputSha,exit_code:Number(exitCode),started_at:envelope.started_at,finished_at:envelope.finished_at};
  validateProjectCommandEnvelope(root,change,command,raw,{revalidateRuntimeIdentity:false});
  const harness=path.join(safeRoot.base,'harness'),hs=fs.lstatSync(harness);if(!hs.isDirectory()||hs.isSymbolicLink())throw Error('unsafe change Harness evidence directory');
  const dir=path.join(harness,'project-command-evidence');if(!fs.existsSync(dir))fs.mkdirSync(dir,{mode:0o755});const ds=fs.lstatSync(dir);if(!ds.isDirectory()||ds.isSymbolicLink())throw Error('unsafe Project Command evidence directory');
  const file=path.join(dir,outputSha.slice(7)+'.json');if(fs.existsSync(file)){const st=fs.lstatSync(file);if(!st.isFile()||st.isSymbolicLink()||!fs.readFileSync(file).equals(raw))throw Error('Project Command evidence digest collision or unsafe destination');return path.relative(root,file).split(path.sep).join('/')}
  const temp=path.join(dir,'.project-command-'+process.pid+'-'+crypto.randomBytes(6).toString('hex'));let linked=false;
  try{const fd=fs.openSync(temp,'wx',0o644);try{fs.writeFileSync(fd,raw);fs.fsyncSync(fd)}finally{fs.closeSync(fd)}try{fs.linkSync(temp,file);linked=true}catch(e){if(e.code!=='EEXIST')throw e;const st=fs.lstatSync(file);if(!st.isFile()||st.isSymbolicLink()||!fs.readFileSync(file).equals(raw))throw Error('Project Command evidence destination changed')}const dfd=fs.openSync(dir,'r');try{fs.fsyncSync(dfd)}finally{fs.closeSync(dfd)}}finally{try{fs.unlinkSync(temp)}catch(e){if(e.code!=='ENOENT')throw e}}
  if(!linked&&!fs.existsSync(file))throw Error('Project Command evidence was not retained');
  return path.relative(root,file).split(path.sep).join('/');
}
function verifyTddEvidenceAt(root,change,requested,options={}){
  const safeRoot=safeChangeRoot(root,change,requested);root=safeRoot.root;const base=safeRoot.base,snapshot=JSON.parse(fs.readFileSync(path.join(base,'harness','ai_snapshot.json'))),rawVerification=JSON.parse(fs.readFileSync(path.join(base,'harness','verification.json'))),planning=planningStateAt(root,change,base),own=(o,k)=>Object.prototype.hasOwnProperty.call(o,k),closed=(o,keys,label)=>{if(!o||typeof o!=='object'||Array.isArray(o)||Object.keys(o).some(k=>!keys.includes(k))||keys.some(k=>!own(o,k)))throw Error(label+' schema mismatch')},isDigest=v=>typeof v==='string'&&/^sha256:[0-9a-f]{64}$/.test(v),checkedAt=Date.now(),date=v=>{const n=Date.parse(v);if(!Number.isFinite(n)||n>checkedAt+300000)throw Error('invalid or future evidence timestamp');return n},same=(a,b)=>canonical(a)===canonical(b),safeFile=p=>{safePattern(p,'evidence path');if(p.includes('*')||p.includes('?'))throw Error('evidence file path cannot be a glob');const f=path.join(root,...p.split('/')),st=fs.lstatSync(f);if(!st.isFile()||st.isSymbolicLink())throw Error('unsafe or missing test file: '+p);return {path:p,mode:(st.mode&0o111)?'100755':'100644',sha256:digest(fs.readFileSync(f))}},secret=/(?:authorization|proxy-authorization|bearer|x-?api-?key|api[_-]?key|token|password|secret|cookie|client[_-]?secret|private[_-]?key|access[_-]?key)[\s:=]+\S+|:\/\/[^/\s:]+:[^/@\s]+@|-----BEGIN [A-Z ]*PRIVATE KEY-----/i,rejectSecrets=v=>{if(typeof v==='string'){if(secret.test(v))throw Error('secret-like TDD evidence');return}if(Array.isArray(v))return v.forEach(rejectSecrets);if(v&&typeof v==='object')Object.values(v).forEach(rejectSecrets)};
  const modern=snapshot.schema_version===3&&rawVerification.schema_version===2,integrated=snapshot.schema_version===4&&rawVerification.schema_version===3;if(!modern&&!integrated||snapshot.planned_change_fingerprint!==planning.planning_fingerprint||snapshot.planned_tdd_policy_sha256!==planning.tdd_policy_sha256||integrated&&snapshot.planned_integration_completeness_sha256!==planning.integration_completeness_sha256||!snapshot.planning_approved_at||!Number.isFinite(Date.parse(snapshot.planning_approved_at))||typeof snapshot.implementation_base_commit!=='string'||!/^[0-9a-f]{40,64}$/.test(snapshot.implementation_base_commit))throw Error('approved planning/TDD/Integration baseline is missing or stale');if(integrated){closed(rawVerification,['schema_version','change_name','migration','tasks'],'verification v3');if(rawVerification.change_name!==change||!Array.isArray(rawVerification.tasks))throw Error('verification v3 identity mismatch')}const verification=integrated?{schema_version:2,change_name:rawVerification.change_name,migration:rawVerification.migration,tasks:rawVerification.tasks.map(t=>({task_id:t.task_id,requirement_refs:t.requirement_refs,changed_paths:t.changed_paths,footprint_observation:t.footprint_observation,commands:t.commands.map(c=>{const x={...c};delete x.surface_ids;delete x.surface_evidence_roles;delete x.surface_probe_bindings;return x})}))}:rawVerification;closed(verification,['schema_version','change_name','migration','tasks'],'verification v2');if(verification.schema_version!==2||verification.change_name!==change||!Array.isArray(verification.tasks))throw Error('verification v2 identity mismatch');if(verification.migration!==null){closed(verification.migration,['from_schema_version','source_sha256','migrated_at','legacy_verification'],'verification migration');if(verification.migration.from_schema_version!==1||!isDigest(verification.migration.source_sha256)||!Number.isFinite(Date.parse(verification.migration.migrated_at))||verification.migration.legacy_verification?.schema_version!==1||verification.migration.legacy_verification?.change_name!==change)throw Error('verification migration invalid')}
  rejectSecrets(verification);const source=options.sourceFingerprint||cp.execFileSync(path.join(root,'scripts','source_fingerprint.sh'),['--kind','source'],{cwd:root,encoding:'utf8'}).trim();if(!isDigest(source))throw Error('invalid expected source fingerprint');const taskMap=new Map,commandKeys=['id','phase','cycle_id','exception_id','kind','argv','working_directory','started_at','finished_at','expected_exit_codes','exit_code','result','source_fingerprint_before','source_fingerprint_after','planning_fingerprint_before','planning_fingerprint_after','tdd_policy_sha256','test_files','expected_failure','observed_summary','output_sha256'];
  for(const t of verification.tasks){closed(t,['task_id','requirement_refs','changed_paths','footprint_observation','commands'],'verification task');if(taskMap.has(t.task_id)||!planning.tasks.has(t.task_id)||!Array.isArray(t.requirement_refs)||!Array.isArray(t.changed_paths)||!Array.isArray(t.commands))throw Error('verification task invalid: '+t.task_id);const ids=new Set;for(const c of t.commands){closed(c,commandKeys,'TDD command');if(typeof c.id!=='string'||!c.id||ids.has(c.id)||!['RED','GREEN','REGRESSION','ALTERNATIVE'].includes(c.phase)||!['build','test','behavior','static'].includes(c.kind)||!Array.isArray(c.argv)||!c.argv.length||c.argv.some(x=>typeof x!=='string')||c.working_directory!=='.'||!Array.isArray(c.expected_exit_codes)||!c.expected_exit_codes.length||!c.expected_exit_codes.every(Number.isInteger)||!Number.isInteger(c.exit_code)||!isDigest(c.source_fingerprint_before)||!isDigest(c.source_fingerprint_after)||!isDigest(c.planning_fingerprint_before)||!isDigest(c.planning_fingerprint_after)||!isDigest(c.tdd_policy_sha256)||!isDigest(c.output_sha256)||typeof c.observed_summary!=='string'||!c.observed_summary.trim()||/[\r\n]/.test(c.observed_summary)||date(c.finished_at)<date(c.started_at))throw Error('TDD command invalid');ids.add(c.id);validateProjectCommandEvidence(root,change,c,{changeRoot:base});if(c.planning_fingerprint_before!==c.planning_fingerprint_after||c.tdd_policy_sha256!==planning.tdd_policy_sha256)throw Error('planning changed during command');if(!Array.isArray(c.test_files))throw Error('test_files array required');if(c.phase==='RED'){if(!c.cycle_id||c.exception_id!==null||!['test','behavior'].includes(c.kind)||c.expected_exit_codes.includes(0)||!c.test_files.length||!c.expected_failure)throw Error('RED contract invalid');closed(c.expected_failure,['class','reason','output_match_sha256','matched'],'expected failure');if(!['assertion','behavior','contract'].includes(c.expected_failure.class)||typeof c.expected_failure.reason!=='string'||!c.expected_failure.reason.trim()||!isDigest(c.expected_failure.output_match_sha256)||typeof c.expected_failure.matched!=='boolean'||!['ExpectedFailure','InvalidRed'].includes(c.result))throw Error('RED failure contract invalid');const valid=c.exit_code!==0&&c.expected_exit_codes.includes(c.exit_code)&&c.expected_failure.matched&&c.source_fingerprint_before===c.source_fingerprint_after;if((c.result==='ExpectedFailure')!==valid)throw Error('RED result mismatch')}else{if(c.expected_failure!==null||c.test_files.length||!['Pass','Fail'].includes(c.result)||(c.result==='Pass')!==c.expected_exit_codes.includes(c.exit_code)||c.source_fingerprint_before!==c.source_fingerprint_after)throw Error(c.phase+' result mismatch');if(c.phase==='ALTERNATIVE'){if(c.cycle_id!==null||typeof c.exception_id!=='string'||!c.exception_id)throw Error('ALTERNATIVE contract invalid')}else if(typeof c.cycle_id!=='string'||!c.cycle_id||c.exception_id!==null)throw Error(c.phase+' cycle contract invalid')}}taskMap.set(t.task_id,t)}
  const exceptionByTask=new Map;for(const x of planning.tdd_policy.exceptions)for(const id of x.task_ids)exceptionByTask.set(id,x);const targetIds=options.taskId?[options.taskId]:[...planning.tasks.keys()],blockingExceptionTaskIds=[];
  for(const id of targetIds){const planned=planning.tasks.get(id);if(!planned)throw Error('unknown task: '+id);if(options.requireDone&&!planned.done)throw Error('task is not checked: '+id);const evidence=taskMap.get(id);if(!evidence||!evidence.commands.length)throw Error('missing TDD evidence for task '+id);const current=c=>c.result==='Pass'&&c.source_fingerprint_after===source&&c.planning_fingerprint_after===planning.planning_fingerprint&&c.tdd_policy_sha256===planning.tdd_policy_sha256,exception=exceptionByTask.get(id);
    if(exception){if(exception.category==='disposable_prototype')throw Error('disposable prototype cannot complete');if(!evidence.changed_paths.length||evidence.changed_paths.some(p=>!matchesPattern(p,exception.paths)))throw Error('TDD exception evidence escaped approved paths: '+id);const unavailable=['unavailable_hardware','unavailable_external_service'].includes(exception.category),alternatives=evidence.commands.filter(c=>c.phase==='ALTERNATIVE'&&c.exception_id===exception.id),recoveries=evidence.commands.filter(c=>c.phase==='REGRESSION'),illegal=evidence.commands.filter(c=>c.phase!=='ALTERNATIVE'&&!(unavailable&&planned.done&&c.phase==='REGRESSION'));if(illegal.length)throw Error('TDD exception task contains an unapproved normal phase: '+id);for(const recovery of recoveries){if(!unavailable||!planned.done)throw Error('only a completed unavailable-environment exception may recover with REGRESSION: '+id);const predecessor=alternatives.some(c=>c.kind===recovery.kind&&c.result==='Pass'&&c.planning_fingerprint_after===planning.planning_fingerprint&&c.tdd_policy_sha256===planning.tdd_policy_sha256&&date(c.finished_at)<=date(recovery.started_at));if(!predecessor)throw Error('environment recovery REGRESSION lacks its prior approved ALTERNATIVE: '+id+'/'+recovery.kind)}const currentRecoveries=recoveries.filter(current),cycles=[...new Set(currentRecoveries.map(c=>c.cycle_id))],recovered=unavailable&&planned.done&&cycles.some(cycle=>planned.verify.every(kind=>currentRecoveries.some(c=>c.cycle_id===cycle&&c.kind===kind)));if(recovered)continue;const alt=alternatives.filter(current);for(const kind of planned.verify)if(!alt.some(c=>c.kind===kind))throw Error('missing current ALTERNATIVE '+kind+' for task '+id);if(unavailable)blockingExceptionTaskIds.push(id);continue}
    const planningCurrent=c=>c.result==='Pass'&&c.planning_fingerprint_after===planning.planning_fingerprint&&c.tdd_policy_sha256===planning.tdd_policy_sha256,reds=evidence.commands.filter(c=>c.phase==='RED'&&c.result==='ExpectedFailure');let closedCycle=false;for(const red of reds){const tests=red.test_files.map(x=>{closed(x,['path','mode','sha256'],'RED test file');if(!isDigest(x.sha256))throw Error('RED test digest invalid');return x}),currentTests=tests.map(x=>safeFile(x.path));if(!same(tests,currentTests))continue;const greens=evidence.commands.filter(c=>c.phase==='GREEN'&&c.cycle_id===red.cycle_id&&c.kind===red.kind&&same(c.argv,red.argv)&&date(c.started_at)>=date(red.finished_at)&&planningCurrent(c)&&c.source_fingerprint_after!==red.source_fingerprint_after);for(const green of greens){const regression=evidence.commands.filter(c=>c.phase==='REGRESSION'&&c.cycle_id===red.cycle_id&&date(c.started_at)>=date(green.finished_at)&&current(c));if(planned.verify.every(kind=>regression.some(c=>c.kind===kind))){closedCycle=true;break}}if(closedCycle)break}if(!closedCycle)throw Error('no current RED -> GREEN -> REGRESSION closure for task '+id)
  }
  return {source_fingerprint:source,planning_fingerprint:planning.planning_fingerprint,tdd_policy_sha256:planning.tdd_policy_sha256,blocking_exception_task_ids:blockingExceptionTaskIds};
}
function verifyTddEvidence(root,change,options={}){
  root=path.resolve(root);return verifyTddEvidenceAt(root,change,path.join(root,'openspec','changes',change),options);
}
function reviewInput(root,change,implementationBase,fingerprints){
  root=path.resolve(root);if(!/^[0-9a-f]{40,64}$/.test(implementationBase))throw Error('full implementation base required');const env={...process.env,GIT_EXTERNAL_DIFF:'',GIT_DIFF_OPTS:''},git=(args,encoding=null)=>cp.execFileSync('git',args,{cwd:root,env,...(encoding?{encoding}:{})}),nul=b=>b.toString('utf8').split('\0').filter(Boolean),policy=loadManifest(root),controlled=p=>policy.isManaged(p)||p==='.gitignore'||p==='ai_snapshot.json'||p==='claude-progress.txt'||p==='session-state.md'||p==='AGENTS.md'||p==='CLAUDE.md'||p==='PROJECT_ATTRIBUTION.md'||p==='openspec'||p.startsWith('openspec/')||p==='.ai-harness'||p.startsWith('.ai-harness/'),excluded=p=>controlled(p)||p==='openspec/changes/archive'||p.startsWith('openspec/changes/archive/'),safePaths=xs=>[...new Set(xs)].filter(p=>{safePattern(p,'review path');if(p.includes('*')||p.includes('?'))throw Error('review path cannot be a glob');if(controlled(p)){try{if(fs.lstatSync(path.join(root,...p.split('/'))).isSymbolicLink())throw Error('controlled review path symlink: '+p)}catch(error){if(error.code!=='ENOENT')throw error}return false}return !excluded(p)}).sort(cmp),head=git(['rev-parse','--verify','HEAD'],'utf8').trim();if(!/^[0-9a-f]{40,64}$/.test(head))throw Error('full HEAD required');
  const tree=rev=>{const out=new Map;for(const row of nul(git(['ls-tree','-r','-z','--full-tree',rev]))){const m=row.match(/^(\d+) \w+ ([0-9a-f]+)\t([\s\S]+)$/);if(m)out.set(m[3],{mode:m[1],oid:m[2]})}return out},index=new Map;for(const row of nul(git(['ls-files','-s','-z']))){const m=row.match(/^(\d+) ([0-9a-f]+) \d+\t([\s\S]+)$/);if(m)index.set(m[3],{mode:m[1],oid:m[2]})}const baseTree=tree(implementationBase),headTree=tree(head),missing={mode:'000000',oid:'<deleted>'},work=p=>{const f=path.join(root,...p.split('/'));let st;try{st=fs.lstatSync(f)}catch(e){if(e.code==='ENOENT')return missing;throw e}if(st.isSymbolicLink()){if(!index.has(p))throw Error('untracked review path symlink: '+p);const target=fs.readlinkSync(f,'buffer');return {mode:'120000',oid:digest(Buffer.concat([Buffer.from('120000\0'),target]))}}if(st.isDirectory()){const idx=index.get(p);if(idx?.mode!=='160000')throw Error('unexpected review directory: '+p);const h=git(['-C',p,'rev-parse','HEAD'],'utf8').trim(),raw=git(['-C',p,'status','--porcelain=v1','-z']);return {mode:'160000',oid:h,status:digest(raw),dirty:raw.length>0}}if(!st.isFile())throw Error('unsupported review path: '+p);return {mode:(st.mode&0o111)?'100755':'100644',oid:digest(fs.readFileSync(f))}},makeLayer=(paths,before,after)=>{paths=safePaths(paths);const records=paths.map(p=>({path:p,before:before(p)||missing,after:after(p)||missing}));return {paths,state_fingerprint:digest(Buffer.from(canonical(records)))}};
  const committedPaths=nul(git(['diff','--no-renames','--name-only','-z',implementationBase,'HEAD','--'])),stagedPaths=nul(git(['diff','--cached','--no-renames','--name-only','-z','HEAD','--'])),unstagedPaths=nul(git(['diff','--no-renames','--name-only','-z','--'])),effectivePaths=nul(git(['diff','--no-renames','--name-only','-z',implementationBase,'--'])),committed=makeLayer(committedPaths,p=>baseTree.get(p),p=>headTree.get(p)),staged=makeLayer(stagedPaths,p=>headTree.get(p),p=>index.get(p)),unstaged=makeLayer(unstagedPaths,p=>index.get(p),work),effective=makeLayer(effectivePaths,p=>baseTree.get(p),work),untrackedPaths=safePaths(nul(git(['ls-files','--others','--exclude-standard','-z']))),untrackedRecords=untrackedPaths.map(p=>({path:p,after:work(p)})),untracked={paths:untrackedPaths,state_fingerprint:digest(Buffer.from(canonical(untrackedRecords)))},gitlinks=[];for(const [p,x]of index)if(x.mode==='160000'&&!excluded(p)){const w=work(p);if(w.dirty)gitlinks.push({path:p,state:w})}const dirty_gitlinks={paths:gitlinks.map(x=>x.path).sort(cmp),state_fingerprint:digest(Buffer.from(canonical(gitlinks)))},layers={effective,committed,staged,unstaged,untracked,dirty_gitlinks},reviewPaths=safePaths(Object.values(layers).flatMap(x=>x.paths));return {schema_version:1,implementation_base_commit:implementationBase,head_commit:head,source_fingerprint:fingerprints.source_fingerprint,artifact_fingerprint:fingerprints.artifact_fingerprint,base_specs_fingerprint:fingerprints.base_specs_fingerprint,layers,review_paths:reviewPaths,git_state_fingerprint:digest(Buffer.from(canonical(layers))),raw_diff_persisted:false};
}
module.exports={loadManifest,parseTaskContracts,parseTddPolicy,planningState,planningStateAt,validateProjectCommandEvidence,persistProjectCommandEvidence,verifyTddEvidence,verifyTddEvidenceAt,reviewInput,canonical,digest,safePattern,matchesPattern};
if(require.main===module){try{const upgrade=process.env.AUTOAI_MANIFEST_ALLOW_TEMPLATE_UPGRADE==='1',result=loadManifest(process.cwd(),{allowTemplateUpgrade:upgrade});if(process.env.AUTOAI_MANIFEST_PRINT_UPGRADE_PATHS==='1')process.stdout.write(result.missing_managed_paths.join('\n')+(result.missing_managed_paths.length?'\n':''));else console.log('Manifest policy passed.')}catch(e){console.error('[ERR] '+e.message);process.exit(6)}}
