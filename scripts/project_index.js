#!/usr/bin/env node
'use strict';
const fs=require('fs'),path=require('path'),cp=require('child_process'),crypto=require('crypto');
const lib=require('./project_profile_lib.js');
const args=process.argv.slice(2),refresh=args.includes('--refresh'),check=args.includes('--check'),json=args.includes('--json');
if(refresh===check||args.some(x=>!['--refresh','--check','--json'].includes(x))){console.error('usage: project_index.sh --refresh|--check [--json]');process.exit(2)}
const root=lib.ensureRepositoryRoot(process.cwd()),file=path.join(root,'.ai-harness','derived','project-index.json');
const sha=value=>'sha256:'+crypto.createHash('sha256').update(value).digest('hex');
const source=()=>cp.execFileSync(path.join(root,'scripts','source_fingerprint.sh'),['--kind','source'],{cwd:root,encoding:'utf8'}).trim();
const nul=buffer=>buffer.toString('utf8').split('\0').filter(Boolean);
const roleRegex=lib.globRegex;
const atomic=(target,value)=>{
  const dir=lib.ensureRepositoryDirectory(root,'.ai-harness/derived','project index derived directory');
  if(path.dirname(target)!==dir)throw Error('project index target escaped its derived directory');
  if(fs.existsSync(target))lib.safeRepositoryEntry(root,'.ai-harness/derived/project-index.json','project index target','file');
  const temp=path.join(dir,`.project-index-${process.pid}-${crypto.randomBytes(4).toString('hex')}`);
  fs.writeFileSync(temp,JSON.stringify(value,null,2)+'\n',{mode:0o600,flag:'wx'});
  lib.safeRepositoryEntry(root,'.ai-harness/derived','project index derived directory','directory');
  fs.renameSync(temp,target);
};
const toolchainContract=(loaded,moduleIds=null)=>{
  const rows=loaded.profile.toolchain_identity.filter(identity=>moduleIds===null||identity.module_ids.some(id=>moduleIds.includes(id))).map(identity=>({
    id:identity.id,module_ids:identity.module_ids,command_id:identity.command_id,
    command_identity_sha256:lib.commandIdentity(loaded,loaded.commands.get(identity.command_id))
  }));
  return sha(Buffer.from(lib.canonical(rows)));
};
const runtimeIdentityCache=new Map();
const runtimeIdentity=(commandId)=>{
  if(runtimeIdentityCache.has(commandId))return runtimeIdentityCache.get(commandId);
  const env={...process.env,AUTOAI_EXECUTION_CONTEXT:'local'};
  delete env.AUTOAI_CI_PROFILE;
  const raw=cp.execFileSync(process.execPath,[
    path.join(root,'scripts','project_command.js'),commandId,'--internal-runtime-identity'
  ],{cwd:root,encoding:'utf8',env,maxBuffer:1048576});
  const value=lib.parseJsonStrict(raw,'toolchain runtime identity');
  const keys=[
    'schema_version','command_id','execution_context','ci_profile_id',
    'toolchain_identity_sha256','platform_identity_sha256',
    'allowed_environment_sha256','executable_inventory_sha256'
  ];
  if(Object.keys(value).sort().join('\0')!==keys.sort().join('\0')||
     value.schema_version!==1||value.command_id!==commandId||
     value.execution_context!=='local'||value.ci_profile_id!==null||
     ![
       value.toolchain_identity_sha256,value.platform_identity_sha256,
       value.allowed_environment_sha256,value.executable_inventory_sha256
     ].every(item=>/^sha256:[0-9a-f]{64}$/.test(item)))
    throw Error('project command returned an invalid runtime identity: '+commandId);
  runtimeIdentityCache.set(commandId,value);
  return value;
};
const toolchainRuntime=(loaded,moduleIds=null)=>{
  const rows=loaded.profile.toolchain_identity
    .filter(identity=>moduleIds===null||identity.module_ids.some(id=>moduleIds.includes(id)))
    .map(identity=>({
      id:identity.id,module_ids:identity.module_ids,command_id:identity.command_id,
      command_identity_sha256:lib.commandIdentity(loaded,loaded.commands.get(identity.command_id)),
      runtime_identity:runtimeIdentity(identity.command_id)
    }));
  return sha(Buffer.from(lib.canonical(rows)));
};
function build(){
  const loaded=lib.parseProfile(root,path.join(root,'.ai-harness','project-profile.json')),before=source();
  const files=[...new Set(nul(cp.execFileSync('git',['ls-files','-co','--exclude-standard','-z'],{cwd:root})))].sort((a,b)=>Buffer.from(a).compare(Buffer.from(b)));
  const detection=JSON.parse(cp.execFileSync(path.join(root,'scripts','project_detect.sh'),['--json'],{cwd:root,encoding:'utf8'}));
  const moduleList=[...loaded.modules.values()].sort((a,b)=>Buffer.from(a.id).compare(Buffer.from(b.id)));
  const moduleDepth=module=>module.root==='.'?0:module.root.split('/').length;
  const contains=(module,file)=>module.root==='.'||file===module.root||file.startsWith(module.root+'/');
  const ownership=new Map(files.map(file=>{
    const matches=moduleList.filter(module=>contains(module,file));
    const depth=Math.max(-1,...matches.map(moduleDepth));
    return [file,matches.filter(module=>moduleDepth(module)===depth).map(module=>module.id)];
  }));
  const ambiguousOwnership=[...ownership].filter(([,owners])=>owners.length>1).map(([file,module_ids])=>({path:file,module_ids}));
  const modules=[],graphOwners=new Map(moduleList.flatMap(module=>module.build_graph_entries.map(entry=>[entry.id,module.id])));
  for(const module of moduleList){
    const moduleFiles=files.filter(file=>(ownership.get(file)||[]).includes(module.id));
    const roles={};
    for(const [role,patterns] of Object.entries(module.path_roles)){
      const regexes=patterns.map(roleRegex);roles[role]=moduleFiles.filter(p=>regexes.some(re=>re.test(p)));
    }
    const approvedOverlaps=module.path_role_overlaps.map(entry=>({...entry,matcher:roleRegex(entry.path)})),pathRoleOverlaps=[];
    for(const file of moduleFiles){const matched=Object.keys(roles).filter(role=>roles[role].includes(file)).sort();if(matched.length<2)continue;const approval=approvedOverlaps.find(entry=>entry.matcher.test(file)&&lib.canonical([...entry.roles].sort())===lib.canonical(matched));if(!approval)throw Error(`module ${module.id} has an unapproved path-role overlap: ${file} -> ${matched.join(',')}`);pathRoleOverlaps.push({path:file,roles:matched,rationale:approval.rationale})}
    const inventory=entry=>{
      const matches=moduleFiles.filter(file=>roleRegex(entry.path).test(file));
      const adapterExport=entry.adapter_export?{
        ...entry.adapter_export,
        command_identity_sha256:lib.commandIdentity(loaded,loaded.commands.get(entry.adapter_export.command_id))
      }:null;
      return {
        ...entry,
        confidence:entry.source==='adapter-export'?'adapter-export-bound':'reviewed-declaration',
        inventory_status:matches.length?'matched':'unmatched',
        inventory_matches:matches,
        ...(adapterExport?{adapter_export:adapterExport}:{})
      };
    };
    modules.push({
      id:module.id,root:module.root,adapter:module.adapter,
      adapter_identity_sha256:sha(Buffer.from(lib.canonical({
        adapter_contract_version:lib.ADAPTER_CONTRACT_VERSION,
        adapter:module.adapter,capabilities:module.capabilities,
        capability_status:module.capability_status,
        build_targets:module.build_targets,
        build_graph_entries:module.build_graph_entries,
        distribution_surfaces:module.distribution_surfaces
      }))),
      toolchain_identity_contract_sha256:toolchainContract(loaded,[module.id]),
      toolchain_identity_runtime_sha256:toolchainRuntime(loaded,[module.id]),
      path_roles:roles,
      path_role_overlaps:pathRoleOverlaps,
      capabilities:module.capabilities,
      build_targets:module.build_targets.map(inventory),
      build_graph_entries:module.build_graph_entries.map(inventory),
      relationships:module.build_graph_entries.flatMap(entry=>[
        ...entry.depends_on.map(target=>({kind:'build-dependency',from:entry.id,from_module_id:module.id,to:target,to_module_id:graphOwners.get(target)})),
        ...entry.tested_by.map(target=>({kind:'tested-by',from:entry.id,from_module_id:module.id,to:target,to_module_id:graphOwners.get(target)})),
        ...entry.consumed_by.map(target=>({kind:'consumed-by',from:entry.id,from_module_id:module.id,to:target,to_module_id:graphOwners.get(target)}))
      ]),
      distribution_surfaces:module.distribution_surfaces.map(surface=>({
        ...surface,
        inventory_status:moduleFiles.some(file=>roleRegex(surface.path).test(file))?'matched':'unmatched',
        build_modules:[...new Set(surface.build_entry_ids.map(id=>graphOwners.get(id)))].sort(),
        consumer_modules:[...new Set(surface.consumer_entry_ids.map(id=>graphOwners.get(id)))].sort()
      })),
      uncertain_candidates:detection.candidates.filter(x=>x.module_root===module.root&&x.adapter===module.adapter)
    });
  }
  const after=source();if(before!==after)throw Error('source changed while building project index');
  return {
    schema_version:1,generated_at:new Date().toISOString(),
    profile_sha256:loaded.profile_sha256,source_fingerprint:after,
    toolchain_identity_contract_sha256:toolchainContract(loaded),
    toolchain_identity_runtime_sha256:toolchainRuntime(loaded),
    index_kind:'adapter-reviewed-plus-uncertain-candidates',
    semantic_guarantee:'advisory-only',
    file_ownership:{rule:'deepest-module-root',ambiguous:ambiguousOwnership},
    modules
  };
}
function validate(value){
  if(!value||value.schema_version!==1||value.index_kind!=='adapter-reviewed-plus-uncertain-candidates'||value.semantic_guarantee!=='advisory-only'||value.file_ownership?.rule!=='deepest-module-root'||!Array.isArray(value.file_ownership?.ambiguous)||!Array.isArray(value.modules)||!/^sha256:[0-9a-f]{64}$/.test(value.profile_sha256)||!/^sha256:[0-9a-f]{64}$/.test(value.source_fingerprint)||!/^sha256:[0-9a-f]{64}$/.test(value.toolchain_identity_contract_sha256)||!/^sha256:[0-9a-f]{64}$/.test(value.toolchain_identity_runtime_sha256)||value.modules.some(module=>!/^sha256:[0-9a-f]{64}$/.test(module.toolchain_identity_contract_sha256)||!/^sha256:[0-9a-f]{64}$/.test(module.toolchain_identity_runtime_sha256)))throw Error('project index schema mismatch');
  const generated=Date.parse(value.generated_at);if(!Number.isFinite(generated)||generated>Date.now()+300000)throw Error('project index timestamp is invalid');
  const expected=build(),withoutTime=x=>({...x,generated_at:null});
  if(lib.canonical(withoutTime(value))!==lib.canonical(withoutTime(expected)))throw Error('project index is stale or its derived content changed');
  return value;
}
try{
  let value;
  if(refresh){value=build();atomic(file,value)}else{const entry=lib.safeRepositoryEntry(root,'.ai-harness/derived/project-index.json','project index','file');value=validate(JSON.parse(fs.readFileSync(entry.path,'utf8')))}
  const result={schema_version:1,status:'fresh',path:'.ai-harness/derived/project-index.json',profile_sha256:value.profile_sha256,source_fingerprint:value.source_fingerprint,toolchain_identity_contract_sha256:value.toolchain_identity_contract_sha256,toolchain_identity_runtime_sha256:value.toolchain_identity_runtime_sha256,module_count:value.modules.length};
  if(json)console.log(JSON.stringify(result,null,2));else console.log(`[OK] project index ${result.status}: ${result.module_count} modules`);
}catch(error){console.error('[ERR] '+error.message);process.exit(6)}
