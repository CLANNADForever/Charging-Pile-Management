#!/usr/bin/env node
'use strict';
const fs=require('fs'),path=require('path'),lib=require('./project_profile_lib.js');
const json=process.argv.includes('--json');if(process.argv.slice(2).some(x=>x!=='--check'&&x!=='--json')||!process.argv.includes('--check')){console.error('usage: organization_policy.sh --check [--json]');process.exit(2)}
const contexts=['local','ci','release'];
const keys=['allow_command_ids','deny_command_ids','allow_capabilities','max_timeout_seconds','inherit_env','allow_side_effects','output_limit_bytes'];
const secretEnv=/(?:TOKEN|SECRET|PASSWORD|PASSWD|API_?KEY|PRIVATE_?KEY|CREDENTIAL|COOKIE|AUTHORIZATION)/i;
const injectionEnv=new Set(['BASH_ENV','ENV','SHELLOPTS','BASHOPTS','LD_PRELOAD','LD_LIBRARY_PATH','NODE_OPTIONS','PYTHONPATH','PERL5OPT','RUBYOPT']);
const closed=(o,w,l)=>{if(!o||typeof o!=='object'||Array.isArray(o)||Object.keys(o).some(k=>!w.includes(k))||w.some(k=>!Object.prototype.hasOwnProperty.call(o,k)))throw Error(l+' schema mismatch')};
const unique=(value,label,predicate)=>{
  if(!Array.isArray(value)||new Set(value).size!==value.length||value.some(item=>typeof item!=='string'||!predicate(item)))
    throw Error(label+' invalid');
};
const commandPattern=value=>value==='*'||/^[a-z][a-z0-9]*(?:-[a-z0-9]+)*$/.test(value);
const envName=value=>/^[A-Z_][A-Z0-9_]*$/.test(value)&&!secretEnv.test(value)&&!injectionEnv.has(value)&&value!=='LANG'&&value!=='LC_ALL'&&!value.startsWith('AUTOAI_');
try{
  const root=lib.ensureRepositoryRoot(process.cwd());
  const entry=lib.safeRepositoryEntry(root,'.ai-harness/organization-policy.json','organization policy','file');
  const d=lib.parseJsonStrict(fs.readFileSync(entry.path,'utf8'),'organization policy');
  closed(d,['schema_version','policy_id','contexts'],'policy');
  if(d.schema_version!==1||typeof d.policy_id!=='string'||!/^[a-z][a-z0-9]*(?:-[a-z0-9]+)*$/.test(d.policy_id))throw Error('policy identity mismatch');
  closed(d.contexts,contexts,'policy contexts');
  for(const name of contexts){
    const x=d.contexts[name];closed(x,keys,'policy '+name);
    unique(x.allow_command_ids,`policy ${name}.allow_command_ids`,commandPattern);
    unique(x.deny_command_ids,`policy ${name}.deny_command_ids`,commandPattern);
    unique(x.allow_capabilities,`policy ${name}.allow_capabilities`,value=>value==='*'||lib.CAPABILITIES.has(value));
    unique(x.inherit_env,`policy ${name}.inherit_env`,envName);
    unique(x.allow_side_effects,`policy ${name}.allow_side_effects`,value=>value==='*'||lib.SIDE_EFFECTS.has(value));
    if(!Number.isInteger(x.max_timeout_seconds)||x.max_timeout_seconds<1||x.max_timeout_seconds>86400||
       !Number.isInteger(x.output_limit_bytes)||x.output_limit_bytes<1024||x.output_limit_bytes>1048576)
      throw Error('policy limits invalid');
  }
  const result={schema_version:1,status:'pass',policy_id:d.policy_id,policy_sha256:lib.digest(Buffer.from(lib.canonical(d)))};
  if(json)console.log(JSON.stringify(result,null,2));else console.log(`[OK] organization policy ${d.policy_id}: ${result.policy_sha256}`);
}catch(error){console.error('[ERR] '+error.message);process.exit(6)}
