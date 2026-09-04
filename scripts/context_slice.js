#!/usr/bin/env node
'use strict';
const fs=require('fs'),path=require('path'),cp=require('child_process'),crypto=require('crypto');
const lib=require('./project_profile_lib.js');
const args=process.argv.slice(2);if(!args.length){console.error('usage: context_slice.sh <change> [--task <id>] [--token-budget <n>] --refresh|--check [--json]');process.exit(2)}
const change=args.shift(),validChange=value=>typeof value==='string'&&/^[a-z][a-z0-9]*(?:-[a-z0-9]+)*$/.test(value)&&value!=='archive'&&!value.startsWith('archive-')&&value!=='stale'&&!value.startsWith('stale-');if(!validChange(change)){console.error('[ERR] invalid change ID');process.exit(2)}
let task=null,budget=12000,refresh=false,check=false,json=false;
while(args.length){const x=args.shift();if(x==='--task')task=args.shift();else if(x==='--token-budget')budget=Number(args.shift());else if(x==='--refresh')refresh=true;else if(x==='--check')check=true;else if(x==='--json')json=true;else{console.error('[ERR] unknown option: '+x);process.exit(2)}}
if(refresh===check||!Number.isInteger(budget)||budget<256||budget>1000000||task!==null&&!/^\d+(?:\.\d+)*$/.test(task)){console.error('[ERR] invalid context slice options');process.exit(2)}
const root=lib.ensureRepositoryRoot(process.cwd()),changeRoot=path.join(root,'openspec','changes',change),target=path.join(root,'.ai-harness','derived','context',change,(task||'change')+'.json');
const sha=value=>'sha256:'+crypto.createHash('sha256').update(value).digest('hex'),source=()=>cp.execFileSync(path.join(root,'scripts','source_fingerprint.sh'),['--kind','source'],{cwd:root,encoding:'utf8'}).trim();
const artifact=()=>cp.execFileSync(path.join(root,'scripts','source_fingerprint.sh'),['--kind','artifact','--change',change],{cwd:root,encoding:'utf8'}).trim();
const indexedEntries=cp.execFileSync('git',['ls-files','-s','-z'],{cwd:root}).toString('utf8').split('\0').filter(Boolean).flatMap(record=>{
  const tab=record.indexOf('\t');if(tab<0)return[];
  const [mode,object]=record.slice(0,tab).split(' '),relative=record.slice(tab+1);
  return [{relative,mode,object}];
});
const gitlinks=new Map(indexedEntries.filter(row=>row.mode==='160000').map(row=>[row.relative,row.object]));
const trackedPaths=new Set(indexedEntries.map(row=>row.relative));
const managedPaths=(()=>{
  const entry=lib.safeRepositoryEntry(root,'.ai-harness/manifest.json','managed manifest','file'),manifest=lib.parseJsonStrict(fs.readFileSync(entry.path,'utf8'),'managed manifest');
  if(!Array.isArray(manifest.managed_paths))throw Error('managed manifest paths are invalid');
  return new Set(manifest.managed_paths.filter(row=>row&&typeof row.path==='string').map(row=>row.path));
})();
const controlled=relative=>relative==='ai_snapshot.json'||relative==='claude-progress.txt'||relative==='session-state.md'||
  relative==='AGENTS.md'||relative==='CLAUDE.md'||relative==='PROJECT_ATTRIBUTION.md'||
  relative==='openspec'||relative.startsWith('openspec/')||relative==='.ai-harness'||relative.startsWith('.ai-harness/')||
  managedPaths.has(relative);
const safeFile=relative=>{
  const full=path.join(root,...relative.split('/'));let linkStat;
  try{linkStat=fs.lstatSync(full)}catch(error){if(error.code!=='ENOENT')throw error}
  if(gitlinks.has(relative)&&linkStat?.isDirectory())
    return {path:relative,bytes:0,sha256:sha(Buffer.from('gitlink\0'+gitlinks.get(relative)))};
  if(linkStat?.isSymbolicLink()){
    if(!trackedPaths.has(relative))throw Error('untracked context path is a symbolic link: '+relative);
    if(controlled(relative))throw Error('controlled context path is a symbolic link: '+relative);
    const target=fs.readlinkSync(full,'buffer');
    return {path:relative,bytes:target.length,sha256:sha(Buffer.concat([Buffer.from('120000\0'),target]))};
  }
  const entry=lib.safeRepositoryEntry(root,relative,'context file','file');
  return {path:relative,bytes:entry.stat.size,sha256:sha(fs.readFileSync(entry.path))};
};
const globRegex=lib.globRegex;
const atomic=(file,value)=>{
  const relativeDir=`.ai-harness/derived/context/${change}`;
  const dir=lib.ensureRepositoryDirectory(root,relativeDir,'context derived directory');
  if(path.dirname(file)!==dir)throw Error('context target escaped its derived directory');
  if(fs.existsSync(file))lib.safeRepositoryEntry(root,path.relative(root,file).split(path.sep).join('/'),'context target','file');
  const temp=path.join(dir,`.slice-${process.pid}-${crypto.randomBytes(4).toString('hex')}`);
  fs.writeFileSync(temp,JSON.stringify(value,null,2)+'\n',{mode:0o600,flag:'wx'});
  lib.safeRepositoryEntry(root,relativeDir,'context derived directory','directory');
  fs.renameSync(temp,file);
};
function build(){
  for(const relative of ['openspec','openspec/changes']){const full=path.join(root,...relative.split('/')),ancestor=fs.lstatSync(full);if(!ancestor.isDirectory()||ancestor.isSymbolicLink())throw Error('unsafe context ancestry: '+relative)}
  const st=fs.lstatSync(changeRoot);if(!st.isDirectory()||st.isSymbolicLink())throw Error('change does not exist safely');
  const loaded=lib.parseProfile(root,path.join(root,'.ai-harness','project-profile.json')),sourceBefore=source(),artifactBefore=artifact();
  const unique=(values)=>[...new Set(values)].sort((a,b)=>Buffer.from(a).compare(Buffer.from(b)));
  const nul=buffer=>buffer.toString('utf8').split('\0').filter(Boolean);
  const repoFiles=unique(nul(cp.execFileSync('git',['ls-files','-co','--exclude-standard','-z'],{cwd:root})));
  const entryPresent=relative=>{try{fs.lstatSync(path.join(root,...relative.split('/')));return true}catch(error){if(error.code==='ENOENT')return false;throw error}};
  const changedFiles=unique([
    ...nul(cp.execFileSync('git',['diff','--name-only','--diff-filter=ACDMRTUXB','-z'],{cwd:root})),
    ...nul(cp.execFileSync('git',['diff','--cached','--name-only','--diff-filter=ACDMRTUXB','-z'],{cwd:root})),
    ...nul(cp.execFileSync('git',['ls-files','--others','--exclude-standard','-z'],{cwd:root}))
  ]);
  const p0=['.ai-harness/project-profile.json',`openspec/changes/${change}/proposal.md`,`openspec/changes/${change}/design.md`,`openspec/changes/${change}/tasks.md`];
  const specRoot=path.join(changeRoot,'specs');if(fs.existsSync(specRoot)){for(const capability of fs.readdirSync(specRoot).sort()){const p=`openspec/changes/${change}/specs/${capability}/spec.md`;if(fs.existsSync(path.join(root,...p.split('/'))))p0.push(p)}}
  const dynamic=JSON.parse(cp.execFileSync(path.join(root,'scripts','change_status.sh'),[change,'--json'],{cwd:root,encoding:'utf8'}));
  for(const p of dynamic.must_read||[])if(typeof p==='string'&&fs.existsSync(path.join(root,...p.split('/'))))p0.push(p);
  const indexFile=path.join(root,'.ai-harness','derived','project-index.json');let index=null,indexStatus='missing';
  try{cp.execFileSync(path.join(root,'scripts','project_index.sh'),['--check','--json'],{cwd:root,stdio:'pipe'});index=JSON.parse(fs.readFileSync(indexFile,'utf8'));indexStatus='fresh'}catch{}
  let selection={
    mode:'change-scoped',task:null,integration_completeness_sha256:null,
    selected_surface_ids:[],surface_paths:[],candidate_diff_paths:[],
    direct_module_ids:[],selected_module_ids:[],
    direct_build_graph_entry_ids:[],one_hop_build_graph_entry_ids:[],
    selected_distribution_surface_ids:[],unmatched_surface_paths:[],risk_signals:[]
  };
  if(task){
    const plan=require(path.join(root,'scripts','integration_surface_lib.js')).parsePlanFromChangeRoot(root,change,changeRoot);
    const taskRow=plan.tasks.get(task);if(!taskRow)throw Error('OpenSpec task does not exist: '+task);
    const surfaces=plan.block.surfaces.filter(surface=>surface.task_ids.includes(task));
    const surfacePaths=unique(surfaces.flatMap(surface=>[
      ...surface.producer_paths,...surface.consumer_paths,
      ...(surface.compatibility?.old_consumer_paths||[]),
      ...(surface.compatibility?.replacement_consumer_paths||[])
    ]));
    const moduleDepth=module=>module.root==='.'?0:module.root.split('/').length;
    const contains=(module,relative)=>module.root==='.'||relative===module.root||relative.startsWith(module.root+'/');
    const directModuleIds=unique(surfacePaths.flatMap(relative=>{
      const matches=index?index.modules.filter(module=>contains(module,relative)):[...loaded.modules.values()].filter(module=>contains(module,relative));
      const depth=Math.max(-1,...matches.map(moduleDepth));
      return matches.filter(module=>moduleDepth(module)===depth).map(module=>module.id);
    }));
    const entryRows=index?index.modules.flatMap(module=>module.build_graph_entries.map(entry=>({module_id:module.id,entry}))):[];
    const entryById=new Map(entryRows.map(row=>[row.entry.id,row]));
    const distributionRows=index?index.modules.flatMap(module=>module.distribution_surfaces.map(surface=>({module_id:module.id,surface}))):[];
    const selectedDistributionRows=distributionRows.filter(row=>{
      const re=globRegex(row.surface.path);
      return surfacePaths.some(relative=>re.test(relative));
    });
    const directEntryIds=new Set(entryRows.filter(row=>{
      const re=globRegex(row.entry.path);
      return surfacePaths.some(relative=>re.test(relative))||
        (row.entry.inventory_matches||[]).some(relative=>surfacePaths.includes(relative));
    }).map(row=>row.entry.id));
    for(const row of selectedDistributionRows)for(const id of [...row.surface.build_entry_ids,...row.surface.consumer_entry_ids])directEntryIds.add(id);
    const relationRows=index?index.modules.flatMap(module=>module.relationships||[]):[];
    const oneHopEntryIds=new Set;
    for(const relation of relationRows){
      if(directEntryIds.has(relation.from)&&!directEntryIds.has(relation.to))oneHopEntryIds.add(relation.to);
      if(directEntryIds.has(relation.to)&&!directEntryIds.has(relation.from))oneHopEntryIds.add(relation.from);
    }
    const selectedEntryIds=unique([...directEntryIds,...oneHopEntryIds]);
    const selectedModuleIds=unique([
      ...directModuleIds,
      ...selectedEntryIds.flatMap(id=>entryById.has(id)?[entryById.get(id).module_id]:[])
    ]);
    const graphPaths=unique(selectedEntryIds.flatMap(id=>{
      const row=entryById.get(id);return row?[row.entry.path,...(row.entry.inventory_matches||[])]:[];
    }));
    const candidateDiffPaths=changedFiles.filter(relative=>
      surfacePaths.includes(relative)||graphPaths.includes(relative));
    const unmatchedSurfacePaths=surfacePaths.filter(relative=>
      !repoFiles.includes(relative)&&!entryRows.some(row=>
        globRegex(row.entry.path).test(relative)||
        (row.entry.inventory_matches||[]).includes(relative)));
    const risks=[];
    const addRisk=(code,severity,details)=>risks.push({code,severity,details});
    if(!surfaces.length)addRisk('no-planned-product-surface','info','The task has no Integration Completeness surface; project context is planning-only.');
    else addRisk('product-surface-scope','info',`${surfaces.length} approved product surface(s) are assigned to this task.`);
    const contractRisk=surfaces.filter(surface=>surface.contract_impact!=='compatible');
    if(contractRisk.length)addRisk('contract-transition','high',`Non-compatible surfaces: ${contractRisk.map(surface=>surface.id).join(', ')}`);
    const boundaryRisk=surfaces.filter(surface=>['external_api','callback_or_plugin','protocol_or_persistence','build_or_install'].includes(surface.kind));
    if(boundaryRisk.length)addRisk('boundary-surface','medium',`Boundary surfaces: ${boundaryRisk.map(surface=>surface.id).join(', ')}`);
    if(indexStatus!=='fresh')addRisk('project-index-unavailable','medium','No fresh project index was available; graph closure is limited to explicit surface paths.');
    if(unmatchedSurfacePaths.length)addRisk('surface-path-not-materialized','medium',`Paths absent from the current project inventory: ${unmatchedSurfacePaths.join(', ')}`);
    const ambiguous=index?.file_ownership?.ambiguous?.filter(row=>surfacePaths.includes(row.path))||[];
    if(ambiguous.length)addRisk('ambiguous-module-ownership','high',`Ambiguous surface paths: ${ambiguous.map(row=>row.path).join(', ')}`);
    if(oneHopEntryIds.size)addRisk('build-graph-one-hop','info',`One-hop graph entries: ${unique([...oneHopEntryIds]).join(', ')}`);
    risks.sort((a,b)=>Buffer.from(a.code+'\0'+a.details).compare(Buffer.from(b.code+'\0'+b.details)));
    selection={
      mode:'task-scoped',
      task:{id:task,requirement_refs:taskRow.refs,verify_kinds:taskRow.verify},
      integration_completeness_sha256:plan.block_sha256,
      selected_surface_ids:surfaces.map(surface=>surface.id),
      surface_paths:surfacePaths,
      candidate_diff_paths:candidateDiffPaths,
      direct_module_ids:directModuleIds,
      selected_module_ids:selectedModuleIds,
      direct_build_graph_entry_ids:unique([...directEntryIds]),
      one_hop_build_graph_entry_ids:unique([...oneHopEntryIds]),
      selected_distribution_surface_ids:unique(selectedDistributionRows.map(row=>row.surface.id)),
      unmatched_surface_paths:unmatchedSurfacePaths,
      risk_signals:risks
    };
  }
  const p1=[];
  if(task){
    p1.push(...selection.surface_paths,...selection.candidate_diff_paths);
    if(index)for(const module of index.modules){
      if(selection.selected_module_ids.includes(module.id))for(const p of module.path_roles.build_metadata||[])if(entryPresent(p))p1.push(p);
      for(const entry of module.build_graph_entries)if([...selection.direct_build_graph_entry_ids,...selection.one_hop_build_graph_entry_ids].includes(entry.id))p1.push(...(entry.inventory_matches||[]));
    }
  }else if(index)for(const module of index.modules)for(const role of ['production','test','example','build_metadata'])for(const p of module.path_roles[role]||[])if(entryPresent(p))p1.push(p);
  const p2=[];
  if(task){
    for(const ref of selection.task.requirement_refs){const main='openspec/'+ref.spec_path;if(fs.existsSync(path.join(root,...main.split('/'))))p2.push(main)}
  }else if(fs.existsSync(specRoot))for(const capability of fs.readdirSync(specRoot).sort()){const main=`openspec/specs/${capability}/spec.md`;if(fs.existsSync(path.join(root,...main.split('/'))))p2.push(main)}
  for(const module of loaded.modules.values())for(const surface of module.distribution_surfaces){
    if(task&&!selection.selected_distribution_surface_ids.includes(surface.id))continue;
    const re=globRegex(surface.path);for(const file of repoFiles)if(re.test(file))p2.push(file)
  }
  const campaigns=path.join(root,'.ai-harness','campaigns');
  if(fs.existsSync(campaigns)){const cs=fs.lstatSync(campaigns);if(!cs.isDirectory()||cs.isSymbolicLink())throw Error('unsafe Campaign directory');for(const name of fs.readdirSync(campaigns).sort()){if(!/^[a-z][a-z0-9-]*\.json$/.test(name))continue;const file=path.join(campaigns,name),st=fs.lstatSync(file);if(!st.isFile()||st.isSymbolicLink())throw Error('unsafe Campaign file');let campaign;try{campaign=JSON.parse(fs.readFileSync(file,'utf8'))}catch{continue}const row=Array.isArray(campaign.dependencies)?campaign.dependencies.find(x=>x?.change_id===change):null;if(!row||!Array.isArray(row.depends_on)||row.depends_on.some(x=>!validChange(x)))continue;const campaignId=name.slice(0,-5),status=JSON.parse(cp.execFileSync(process.execPath,[path.join(root,'scripts','campaign.js'),campaignId,'--status','--json'],{cwd:root,encoding:'utf8'}));for(const upstream of row.depends_on){const matches=Array.isArray(status.nodes)?status.nodes.filter(node=>node?.change_id===upstream):[];if(matches.length!==1||!['active','archived'].includes(matches[0].state)||typeof matches[0].path!=='string')throw Error(`Campaign dependency ${upstream} has no validated active/archive identity`);if(matches[0].state==='archived'&&!/^sha256:[0-9a-f]{64}$/.test(matches[0].archive_receipt_sha256||''))throw Error(`Campaign dependency ${upstream} has no trusted archive receipt`);const dependencyRoot=matches[0].path;for(const artifactName of ['proposal.md','design.md','tasks.md']){const rel=`${dependencyRoot}/${artifactName}`,entry=lib.safeRepositoryEntry(root,rel,'Campaign dependency artifact','file');if(!entry.stat.isFile())throw Error(`Campaign dependency artifact is unsafe: ${rel}`);p2.push(rel)}}}}
  const p3=['debt-register.md','defect-rca.md',`openspec/changes/${change}/harness/defect-rca.md`];
  if(index)p3.push('.ai-harness/derived/project-index.json');
  const expand=values=>{
    const out=[],walk=relative=>{
      if(gitlinks.has(relative)){out.push(relative);return}
      const full=path.join(root,...relative.split('/'));let st;try{st=fs.lstatSync(full)}catch(error){if(error.code==='ENOENT')return;throw error}
      if(st.isSymbolicLink()){
        if(!trackedPaths.has(relative)||controlled(relative))throw Error('context path is an unsafe symbolic link: '+relative);
        out.push(relative);return
      }
      if(st.isDirectory())for(const name of fs.readdirSync(full).sort())walk(relative+'/'+name);
      else if(st.isFile())out.push(relative);
      else throw Error('context path has an unsupported type: '+relative);
    };
    for(const raw of unique(values)){if(typeof raw!=='string'||!raw)continue;const relative=raw.replace(/\/+$/,'');lib.repoRelative(relative,'context path');walk(relative)}
    return unique(out);
  };
  const tiers=[{tier:'P0',required:true,items:expand(p0).map(safeFile)},{tier:'P1',required:false,items:expand(p1).map(safeFile)},{tier:'P2',required:false,items:expand(p2).map(safeFile)},{tier:'P3',required:false,items:expand(p3).map(safeFile)}];
  let used=0;for(const tier of tiers)for(const item of tier.items){item.estimated_tokens=Math.max(1,Math.ceil(item.bytes/4));if(tier.required||used+item.estimated_tokens<=budget){item.included=true;used+=item.estimated_tokens}else item.included=false}
  const required=tier=>tier.items.filter(x=>x.included),requiredTokens=tiers[0].items.reduce((n,x)=>n+x.estimated_tokens,0);
  if(requiredTokens>budget)throw Error(`token budget ${budget} cannot contain mandatory P0 context (${requiredTokens} estimated tokens)`);
  const sourceAfter=source(),artifactAfter=artifact();if(sourceBefore!==sourceAfter||artifactBefore!==artifactAfter)throw Error('project or planning changed while building context slice');
  return {schema_version:1,change_name:change,task_id:task,generated_at:new Date().toISOString(),token_budget:budget,token_estimator:'utf8-bytes-divided-by-4-heuristic',token_estimator_error:'model tokenizer is not consulted; estimates are advisory and may differ from actual token use',estimated_tokens:used,profile_sha256:loaded.profile_sha256,source_fingerprint:sourceAfter,artifact_fingerprint:artifactAfter,project_index_status:indexStatus,scope:'reading-order-only; never replaces complete diff or untracked review',selection,tiers};
}
function validate(value){
  if(!value||value.schema_version!==1||value.change_name!==change||value.task_id!==task||value.scope!=='reading-order-only; never replaces complete diff or untracked review'||value.token_estimator!=='utf8-bytes-divided-by-4-heuristic'||typeof value.token_estimator_error!=='string'||!value.selection||value.selection.mode!==(task?'task-scoped':'change-scoped')||!Array.isArray(value.selection.risk_signals)||!Array.isArray(value.tiers)||value.tiers.map(x=>x.tier).join(',')!=='P0,P1,P2,P3')throw Error('context slice is stale or invalid');
  const generated=Date.parse(value.generated_at);if(!Number.isFinite(generated)||generated>Date.now()+300000)throw Error('context slice timestamp is invalid');
  const expected=build(),withoutTime=x=>({...x,generated_at:null});
  if(lib.canonical(withoutTime(value))!==lib.canonical(withoutTime(expected)))throw Error('context slice is stale or its derived content changed');
  return value;
}
try{let value;if(refresh){value=build();atomic(target,value)}else{const entry=lib.safeRepositoryEntry(root,path.relative(root,target).split(path.sep).join('/'),'context slice','file');value=validate(JSON.parse(fs.readFileSync(entry.path,'utf8')))}const result={schema_version:1,status:'fresh',path:path.relative(root,target).split(path.sep).join('/'),estimated_tokens:value.estimated_tokens,project_index_status:value.project_index_status,selection_mode:value.selection.mode,risk_signal_count:value.selection.risk_signals.length};if(json)console.log(JSON.stringify(result,null,2));else console.log(`[OK] context slice ${result.path} (${result.estimated_tokens} estimated tokens; index=${result.project_index_status}; selection=${result.selection_mode})`)}catch(error){console.error('[ERR] '+error.message);process.exit(6)}
