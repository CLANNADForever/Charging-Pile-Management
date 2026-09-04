#!/usr/bin/env node
'use strict';

const fs = require('fs');
const path = require('path');
const crypto = require('crypto');

const ADAPTERS = new Set(['cmake','meson','bazel','make','autotools','xmake','qmake','ninja','custom']);
const ADAPTER_CONTRACT_VERSION = 1;
const CAPABILITIES = new Set([
  'configure','build','test','install','package','consumer',
  'static-analysis','target-run'
]);
const PATH_ROLES = ['production','test','example','generated','vendor','build_metadata'];
const SIDE_EFFECTS = new Set(['workspace-write','network','install','device-write','remote-write']);
const SECRET_ENV = /(?:TOKEN|SECRET|PASSWORD|PASSWD|API_?KEY|PRIVATE_?KEY|CREDENTIAL|COOKIE|AUTHORIZATION)/i;
const INJECTION_ENV = new Set(['BASH_ENV','ENV','SHELLOPTS','BASHOPTS','LD_PRELOAD','LD_LIBRARY_PATH','NODE_OPTIONS','PYTHONPATH','PERL5OPT','RUBYOPT']);
const SECRET_ARG = /(?:authorization|proxy-authorization|bearer|x-?api-?key|api[_-]?key|token|password|secret|cookie|client[_-]?secret|private[_-]?key|access[_-]?key)[\s:=]+\S+|:\/\/[^/\s:]+:[^/@\s]+@|-----BEGIN [A-Z ]*PRIVATE KEY-----/i;
const CREDENTIAL_OPTIONS = new Set(['--token','--password','--secret','--api-key','--apikey','-H','--header','--cookie','--authorization']);

function cmp(a,b){ return Buffer.from(a).compare(Buffer.from(b)); }
function canonical(value){
  if(Array.isArray(value)) return '['+value.map(canonical).join(',')+']';
  if(value && typeof value === 'object')
    return '{'+Object.keys(value).sort(cmp).map(k=>JSON.stringify(k)+':'+canonical(value[k])).join(',')+'}';
  return JSON.stringify(value);
}
function digest(value){
  const bytes = Buffer.isBuffer(value) ? value : Buffer.from(String(value));
  return 'sha256:'+crypto.createHash('sha256').update(bytes).digest('hex');
}
function digestFile(file,maxBytes=268435456){
  const st=fs.lstatSync(file);
  if(!st.isFile()||st.isSymbolicLink())throw new Error('digest input must be a non-symlink regular file');
  if(st.size>maxBytes)throw new Error('digest input exceeds the supported size limit');
  const hash=crypto.createHash('sha256'),buffer=Buffer.allocUnsafe(1024*1024),fd=fs.openSync(file,'r');
  try{let offset=0,read;do{read=fs.readSync(fd,buffer,0,buffer.length,offset);if(read){hash.update(buffer.subarray(0,read));offset+=read}}while(read)}
  finally{fs.closeSync(fd)}
  return 'sha256:'+hash.digest('hex');
}
function closed(value, keys, label){
  if(!value || typeof value !== 'object' || Array.isArray(value) ||
     Object.keys(value).some(k=>!keys.includes(k)) ||
     keys.some(k=>!Object.prototype.hasOwnProperty.call(value,k)))
    throw new Error(label+' schema mismatch');
}
function closedOptional(value,allowed,required,label){
  if(!value || typeof value !== 'object' || Array.isArray(value) ||
     Object.keys(value).some(k=>!allowed.includes(k)) ||
     required.some(k=>!Object.prototype.hasOwnProperty.call(value,k)))
    throw new Error(label+' schema mismatch');
}
function parseJsonStrict(source,label='JSON'){
  let index=0;
  const whitespace=()=>{while(/\s/.test(source[index]||''))index++};
  const string=()=>{
    const start=index++;
    for(;index<source.length;index++){
      if(source[index]==='\\'){index++;continue}
      if(source[index]==='"'){index++;return JSON.parse(source.slice(start,index))}
    }
    throw new Error(label+' contains an unterminated string');
  };
  const value=()=>{
    whitespace();
    if(source[index]==='"')return string();
    if(source[index]==='{'){
      index++;const object=Object.create(null),seen=new Set;whitespace();
      if(source[index]==='}'){index++;return object}
      for(;;){
        whitespace();if(source[index]!=='"')throw new Error(label+' object key expected');
        const key=string();if(seen.has(key))throw new Error(label+' contains duplicate key: '+key);seen.add(key);
        whitespace();if(source[index++]!==':')throw new Error(label+' colon expected');
        Object.defineProperty(object,key,{value:value(),enumerable:true,writable:true,configurable:true});whitespace();
        const delimiter=source[index++];if(delimiter==='}')return object;if(delimiter!==',')throw new Error(label+' comma expected');
      }
    }
    if(source[index]==='['){
      index++;const array=[];whitespace();if(source[index]===']'){index++;return array}
      for(;;){array.push(value());whitespace();const delimiter=source[index++];if(delimiter===']')return array;if(delimiter!==',')throw new Error(label+' comma expected')}
    }
    const match=source.slice(index).match(/^(true|false|null|-?(?:0|[1-9]\d*)(?:\.\d+)?(?:[eE][+-]?\d+)?)/);
    if(!match)throw new Error(label+' contains invalid JSON');index+=match[0].length;return JSON.parse(match[0]);
  };
  const result=value();whitespace();if(index!==source.length)throw new Error(label+' contains trailing data');return result;
}
function identifier(value,label){
  if(typeof value !== 'string' || !/^[a-z][a-z0-9]*(?:-[a-z0-9]+)*$/.test(value))
    throw new Error(label+' must be a kebab-case identifier');
  return value;
}
function nonempty(value,label){
  if(typeof value !== 'string' || !value.trim() || /[\0\r\n]/.test(value))
    throw new Error(label+' must be a non-empty single-line string');
  return value;
}
function repoRelative(value,label,{allowDot=true,allowGlob=false}={}){
  nonempty(value,label);
  if(path.posix.isAbsolute(value) || value.includes('\\') || value.includes('\0') ||
     value.includes('\n') || value.includes('\r') || value.startsWith('!') ||
     value.startsWith(':'))
    throw new Error(label+' is not a safe repository-relative path');
  const probe = allowGlob ? value.replace(/[?*]+/g,'x') : value;
  if(!allowGlob && /[?*]/.test(value)) throw new Error(label+' cannot be a glob');
  const normalized = path.posix.normalize(probe);
  if(normalized!==probe || (!allowDot && normalized === '.') || normalized === '..' || normalized.startsWith('../') ||
     normalized === '.git' || normalized.startsWith('.git/'))
    throw new Error(label+' escapes the repository or targets Git metadata');
  return value;
}
function ensureRepositoryRoot(root){
  root = fs.realpathSync(path.resolve(root));
  const git = path.join(root,'.git');
  const st = fs.lstatSync(git);
  if(!(st.isDirectory() || st.isFile()) || st.isSymbolicLink()) throw new Error('root is not a safe Git worktree');
  return root;
}
function safeRepositoryEntry(root,relative,label,kind='file'){
  repoRelative(relative,label);
  const target=path.resolve(root,...relative.split('/'));
  if(target!==root&&!target.startsWith(root+path.sep))throw new Error(label+' escapes repository');
  let cursor=root;
  const parts=relative==='.'?[]:relative.split('/');
  for(let index=0;index<parts.length;index++){
    cursor=path.join(cursor,parts[index]);
    let st;try{st=fs.lstatSync(cursor)}catch(error){if(error.code==='ENOENT')throw new Error(label+' is missing');throw error}
    if(st.isSymbolicLink())throw new Error(label+' has symbolic-link ancestry');
    if(index<parts.length-1&&!st.isDirectory())throw new Error(label+' has non-directory ancestry');
  }
  const st=fs.lstatSync(target);
  if(kind==='file'&&!st.isFile()||kind==='directory'&&!st.isDirectory()||st.isSymbolicLink())
    throw new Error(label+' must be a non-symlink '+kind);
  const real=fs.realpathSync(target);
  if(real!==root&&!real.startsWith(root+path.sep))throw new Error(label+' resolves outside repository');
  return {path:target,real,stat:st};
}
function ensureRepositoryDirectory(root,relative,label){
  repoRelative(relative,label);
  let cursor=root;
  for(const part of relative==='.'?[]:relative.split('/')){
    cursor=path.join(cursor,part);
    try{
      const st=fs.lstatSync(cursor);
      if(!st.isDirectory()||st.isSymbolicLink())throw new Error(label+' has unsafe directory ancestry');
    }catch(error){
      if(error.code!=='ENOENT')throw error;
      fs.mkdirSync(cursor,{mode:0o700});
      const st=fs.lstatSync(cursor);
      if(!st.isDirectory()||st.isSymbolicLink())throw new Error(label+' could not be created safely');
    }
    const real=fs.realpathSync(cursor);
    if(real!==root&&!real.startsWith(root+path.sep))throw new Error(label+' resolves outside repository');
  }
  return cursor;
}
function ensureDirectory(root,relative,label){
  const entry=safeRepositoryEntry(root,relative,label,'directory');
  return path.relative(root,entry.real).split(path.sep).join('/') || '.';
}
function arrayOfStrings(value,label,{allowEmpty=true}={}){
  if(!Array.isArray(value) || (!allowEmpty && !value.length)) throw new Error(label+' must be an array');
  const out = value.map((v,i)=>nonempty(v,`${label}[${i}]`));
  if(new Set(out).size !== out.length) throw new Error(label+' contains duplicates');
  return out;
}
function commandExecutable(value,label){
  nonempty(value,label);
  if(value.startsWith('./')){
    repoRelative(value.slice(2),label,{allowDot:false});
  }else if(!/^[A-Za-z0-9][A-Za-z0-9._+-]*$/.test(value)){
    throw new Error(label+' must be a PATH command or ./repository/path');
  }
  return value;
}
function moduleContains(moduleRoot,cwd){
  return moduleRoot === '.' || cwd === moduleRoot || cwd.startsWith(moduleRoot+'/');
}
function moduleOwnsPath(moduleRoot,value){
  if(moduleRoot==='.')return true;
  return value===moduleRoot||value.startsWith(moduleRoot+'/');
}
function globRegex(pattern){
  let out='^';
  for(let i=0;i<pattern.length;i++){
    const c=pattern[i];
    if(c==='*'&&pattern[i+1]==='*'){
      const segmentStart=i===0||pattern[i-1]==='/';
      if(segmentStart&&pattern[i+2]==='/'){out+='(?:.*/)?';i+=2}
      else if(segmentStart&&i+2===pattern.length){out+='.*';i++}
      else{out+='[^/]*';i++}
    }else if(c==='*')out+='[^/]*';
    else if(c==='?')out+='[^/]';
    else out+='\\.^$+{}()|[]'.includes(c)?'\\'+c:c;
  }
  return new RegExp(out+'$');
}

function parseProfile(root,file){
  root = ensureRepositoryRoot(root);
  const profilePath=path.resolve(file),inside=profilePath===root||profilePath.startsWith(root+path.sep);
  let profileEntry;
  if(inside)profileEntry=safeRepositoryEntry(root,path.relative(root,profilePath).split(path.sep).join('/')||'.','Project Profile','file');
  else{
    const st=fs.lstatSync(profilePath);
    if(!st.isFile()||st.isSymbolicLink())throw new Error('Project Profile must be a non-symlink regular file');
    profileEntry={path:profilePath,real:fs.realpathSync(profilePath),stat:st};
  }
  const profile = parseJsonStrict(fs.readFileSync(profileEntry.path,'utf8'),'Project Profile');
  closed(profile,['schema_version','modules','commands','toolchain_identity'],'Project Profile');
  if(profile.schema_version !== 1) throw new Error('unsupported Project Profile schema_version');
  if(!Array.isArray(profile.modules) || !profile.modules.length) throw new Error('Project Profile needs at least one module');
  if(!Array.isArray(profile.commands) || !Array.isArray(profile.toolchain_identity))
    throw new Error('Project Profile commands/toolchain_identity must be arrays');

  const modules = new Map(),targetIds=new Set(),graphIds=new Set(),graphRecords=[],distributionIds=new Set(),adapterExportRecords=[],adapterExportCache=new Map();
  for(const [index,item] of profile.modules.entries()){
    const label=`module[${index}]`;
    closedOptional(item,[
      'id','root','adapter','cpp_standards','compilers','target_platforms',
      'path_roles','path_role_overlaps','capabilities','capability_status',
      'build_targets','build_graph_entries','distribution_surfaces'
    ],[
      'id','root','adapter','cpp_standards','compilers','target_platforms',
      'path_roles','capabilities','build_graph_entries','distribution_surfaces'
    ],label);
    const id=identifier(item.id,label+'.id');
    if(modules.has(id)) throw new Error('duplicate module ID: '+id);
    const moduleRoot=ensureDirectory(root,item.root,label+'.root');
    if(!ADAPTERS.has(item.adapter)) throw new Error('unsupported adapter: '+item.adapter);
    arrayOfStrings(item.cpp_standards,label+'.cpp_standards',{allowEmpty:false});
    arrayOfStrings(item.compilers,label+'.compilers',{allowEmpty:false});
    arrayOfStrings(item.target_platforms,label+'.target_platforms',{allowEmpty:false});
    closed(item.path_roles,PATH_ROLES,label+'.path_roles');
    for(const role of PATH_ROLES){
      const patterns=arrayOfStrings(item.path_roles[role],`${label}.path_roles.${role}`);
      patterns.forEach((p,i)=>{
        repoRelative(p,`${label}.path_roles.${role}[${i}]`,{allowGlob:true});
        if(!moduleOwnsPath(moduleRoot,p))throw new Error(`${label}.path_roles.${role}[${i}] is outside module root`);
      });
    }
    const pathRoleOverlaps=(item.path_role_overlaps||[]).map((entry,i)=>{
      closed(entry,['path','roles','rationale'],`${label}.path_role_overlaps[${i}]`);
      repoRelative(entry.path,`${label}.path_role_overlaps[${i}].path`,{allowGlob:true});
      if(!moduleOwnsPath(moduleRoot,entry.path))throw new Error(`${label}.path_role_overlaps[${i}] is outside module root`);
      const roles=arrayOfStrings(entry.roles,`${label}.path_role_overlaps[${i}].roles`,{allowEmpty:false});
      if(roles.length<2||roles.some(role=>!PATH_ROLES.includes(role)))throw new Error(`${label}.path_role_overlaps[${i}] must name at least two path roles`);
      nonempty(entry.rationale,`${label}.path_role_overlaps[${i}].rationale`);
      return {...entry,roles};
    });
    if(!item.capabilities || typeof item.capabilities!=='object' || Array.isArray(item.capabilities))
      throw new Error(label+'.capabilities must be an object');
    for(const [capability,ids] of Object.entries(item.capabilities)){
      if(!CAPABILITIES.has(capability)) throw new Error('unknown capability: '+capability);
      arrayOfStrings(ids,`${label}.capabilities.${capability}`,{allowEmpty:false})
        .forEach((v,i)=>identifier(v,`${label}.capabilities.${capability}[${i}]`));
    }
    const capabilityStatus=item.capability_status||{};
    if(typeof capabilityStatus!=='object'||Array.isArray(capabilityStatus))
      throw new Error(label+'.capability_status must be an object');
    for(const [capability,state] of Object.entries(capabilityStatus)){
      if(!CAPABILITIES.has(capability))throw new Error(label+'.capability_status has an unknown capability: '+capability);
      closed(state,['status','reason'],`${label}.capability_status.${capability}`);
      if(!['available','unavailable','not-applicable','needs-approval'].includes(state.status))
        throw new Error(`${label}.capability_status.${capability}.status is invalid`);
      nonempty(state.reason,`${label}.capability_status.${capability}.reason`);
      const exposed=Object.prototype.hasOwnProperty.call(item.capabilities,capability);
      if(exposed&&state.status!=='available')
        throw new Error(`${label}.capability_status.${capability} must be available while commands are exposed`);
      if(!exposed&&state.status==='available')
        throw new Error(`${label}.capability_status.${capability} cannot be available without an exposed command`);
    }
    if(item.build_targets!==undefined&&!Array.isArray(item.build_targets) || !Array.isArray(item.build_graph_entries) || !Array.isArray(item.distribution_surfaces))
      throw new Error(label+' target/graph/distribution fields must be arrays');
    const parseAdapterExport=(entry,entryLabel,entryType)=>{
      if(entry.source==='profile'){
        if(entry.adapter_export!==undefined)throw new Error(entryLabel+' profile source cannot carry adapter_export provenance');
        return null;
      }
      if(entry.source!=='adapter-export')throw new Error(entryLabel+' source must be profile or adapter-export');
      closed(entry.adapter_export,['schema_version','adapter','command_id','artifact_path','artifact_sha256'],entryLabel+'.adapter_export');
      const provenance=entry.adapter_export;
      if(provenance.schema_version!==1||provenance.adapter!==item.adapter)throw new Error(entryLabel+' adapter export identity mismatch');
      identifier(provenance.command_id,entryLabel+'.adapter_export.command_id');
      repoRelative(provenance.artifact_path,entryLabel+'.adapter_export.artifact_path');
      if(!moduleOwnsPath(moduleRoot,provenance.artifact_path))throw new Error(entryLabel+' adapter export artifact is outside module root');
      if(!/^sha256:[0-9a-f]{64}$/.test(provenance.artifact_sha256))throw new Error(entryLabel+' adapter export digest is invalid');
      const artifact=safeRepositoryEntry(root,provenance.artifact_path,entryLabel+'.adapter_export.artifact_path','file');
      const modulePath=path.join(root,...moduleRoot.split('/'));
      if(artifact.real!==modulePath&&!artifact.real.startsWith(modulePath+path.sep))
        throw new Error(entryLabel+' adapter export artifact resolves outside module root');
      if(digestFile(artifact.path,67108864)!==provenance.artifact_sha256)
        throw new Error(entryLabel+' adapter export artifact is missing, unsafe or stale');
      const cacheKey=artifact.real+'\0'+provenance.artifact_sha256;
      let exported=adapterExportCache.get(cacheKey);
      if(!exported){
        exported=parseJsonStrict(fs.readFileSync(artifact.path,'utf8'),entryLabel+' adapter export');
        closed(exported,['schema_version','adapter','module_id','rows'],entryLabel+' adapter export');
        if(exported.schema_version!==1||!Array.isArray(exported.rows))
          throw new Error(entryLabel+' adapter export document identity mismatch');
        const rowKeys=new Set();
        exported.rows=exported.rows.map((row,rowIndex)=>{
          const rowLabel=`${entryLabel} adapter export row[${rowIndex}]`;
          if(row?.entry_type==='build-target'){
            closed(row,['entry_type','id','kind','name','path'],rowLabel);
            identifier(row.id,rowLabel+'.id');nonempty(row.kind,rowLabel+'.kind');nonempty(row.name,rowLabel+'.name');
            repoRelative(row.path,rowLabel+'.path',{allowGlob:true});
          }else if(row?.entry_type==='build-graph-entry'){
            closed(row,['entry_type','id','kind','path','depends_on','tested_by','consumed_by'],rowLabel);
            identifier(row.id,rowLabel+'.id');nonempty(row.kind,rowLabel+'.kind');repoRelative(row.path,rowLabel+'.path',{allowGlob:true});
            for(const relation of ['depends_on','tested_by','consumed_by'])
              row[relation]=arrayOfStrings(row[relation],rowLabel+'.'+relation).map((value,index)=>identifier(value,`${rowLabel}.${relation}[${index}]`));
          }else throw new Error(rowLabel+' has an unknown entry_type');
          if(!moduleOwnsPath(moduleRoot,row.path))throw new Error(rowLabel+' is outside module root');
          const rowKey=row.entry_type+'\0'+row.id;if(rowKeys.has(rowKey))throw new Error(entryLabel+' adapter export contains a duplicate row');
          rowKeys.add(rowKey);return row;
        });
        adapterExportCache.set(cacheKey,exported);
      }
      if(exported.adapter!==item.adapter||exported.module_id!==id)
        throw new Error(entryLabel+' adapter export document identity mismatch');
      const expected=entryType==='build-target'
        ? {entry_type:entryType,id:entry.id,kind:entry.kind,name:entry.name,path:entry.path}
        : {entry_type:entryType,id:entry.id,kind:entry.kind,path:entry.path,depends_on:entry.depends_on,tested_by:entry.tested_by,consumed_by:entry.consumed_by};
      if(!exported.rows.some(row=>canonical(row)===canonical(expected)))
        throw new Error(entryLabel+' adapter export does not contain the declared row');
      adapterExportRecords.push({module_id:id,provenance,label:entryLabel});
      return provenance;
    };
    const buildTargets=(item.build_targets||[]).map((entry,i)=>{
      closedOptional(entry,['id','kind','name','path','source','adapter_export'],['id','kind','name','path','source'],`${label}.build_targets[${i}]`);
      identifier(entry.id,`${label}.build_targets[${i}].id`);
      if(targetIds.has(entry.id))throw new Error('duplicate build target ID: '+entry.id);targetIds.add(entry.id);
      nonempty(entry.kind,`${label}.build_targets[${i}].kind`);
      nonempty(entry.name,`${label}.build_targets[${i}].name`);
      repoRelative(entry.path,`${label}.build_targets[${i}].path`,{allowGlob:true});
      if(!moduleOwnsPath(moduleRoot,entry.path))throw new Error(`${label}.build_targets[${i}] is outside module root`);
      parseAdapterExport(entry,`${label}.build_targets[${i}]`,'build-target');
      return entry;
    });
    const graphEntries=item.build_graph_entries.map((entry,i)=>{
      closedOptional(entry,['id','kind','path','source','depends_on','tested_by','consumed_by','adapter_export'],['id','kind','path','source'],`${label}.build_graph_entries[${i}]`);
      identifier(entry.id,`${label}.build_graph_entries[${i}].id`);
      if(graphIds.has(entry.id))throw new Error('duplicate build graph entry ID: '+entry.id);graphIds.add(entry.id);
      nonempty(entry.kind,`${label}.build_graph_entries[${i}].kind`);
      repoRelative(entry.path,`${label}.build_graph_entries[${i}].path`,{allowGlob:true});
      if(!moduleOwnsPath(moduleRoot,entry.path))throw new Error(`${label}.build_graph_entries[${i}] is outside module root`);
      const normalized={...entry};
      for(const relation of ['depends_on','tested_by','consumed_by'])normalized[relation]=arrayOfStrings(entry[relation]||[],`${label}.build_graph_entries[${i}].${relation}`).map((value,index)=>identifier(value,`${label}.build_graph_entries[${i}].${relation}[${index}]`));
      parseAdapterExport(normalized,`${label}.build_graph_entries[${i}]`,'build-graph-entry');
      graphRecords.push({module_id:id,entry:normalized,label:`${label}.build_graph_entries[${i}]`});
      return normalized;
    });
    const distributionSurfaces=item.distribution_surfaces.map((entry,i)=>{
      closedOptional(entry,['id','kind','path','build_entry_ids','consumer_entry_ids'],['id','kind','path'],`${label}.distribution_surfaces[${i}]`);
      identifier(entry.id,`${label}.distribution_surfaces[${i}].id`);
      if(distributionIds.has(entry.id))throw new Error('duplicate distribution surface ID: '+entry.id);distributionIds.add(entry.id);
      nonempty(entry.kind,`${label}.distribution_surfaces[${i}].kind`);
      repoRelative(entry.path,`${label}.distribution_surfaces[${i}].path`,{allowGlob:true});
      if(!moduleOwnsPath(moduleRoot,entry.path))throw new Error(`${label}.distribution_surfaces[${i}] is outside module root`);
      const normalized={...entry};
      for(const relation of ['build_entry_ids','consumer_entry_ids'])normalized[relation]=arrayOfStrings(entry[relation]||[],`${label}.distribution_surfaces[${i}].${relation}`).map((value,index)=>identifier(value,`${label}.distribution_surfaces[${i}].${relation}[${index}]`));
      return normalized;
    });
    modules.set(id,{
      ...item,root:moduleRoot,path_role_overlaps:pathRoleOverlaps,
      capability_status:capabilityStatus,
      build_targets:buildTargets,build_graph_entries:graphEntries,
      distribution_surfaces:distributionSurfaces
    });
  }
  for(const record of graphRecords)for(const relation of ['depends_on','tested_by','consumed_by'])for(const target of record.entry[relation])if(!graphIds.has(target)||target===record.entry.id)throw new Error(`${record.label}.${relation} references an unknown or self graph entry: ${target}`);
  {
    const dependencies=new Map(graphRecords.map(record=>[record.entry.id,record.entry.depends_on])),visiting=new Set,visited=new Set;
    const visit=id=>{if(visiting.has(id))throw new Error('build graph depends_on contains a cycle at '+id);if(visited.has(id))return;visiting.add(id);for(const dependency of dependencies.get(id)||[])visit(dependency);visiting.delete(id);visited.add(id)};
    for(const id of dependencies.keys())visit(id);
  }
  for(const module of modules.values())for(const surface of module.distribution_surfaces)for(const relation of ['build_entry_ids','consumer_entry_ids'])for(const target of surface[relation])if(!graphIds.has(target))throw new Error(`distribution surface ${surface.id} references unknown graph entry: ${target}`);

  const commands = new Map();
  for(const [index,item] of profile.commands.entries()){
    const label=`command[${index}]`;
    closedOptional(item,[
      'id','module_ids','capability','argv','cwd','timeout_seconds',
      'inherit_env','output_roles','side_effects','required_tools'
    ],[
      'id','module_ids','capability','argv','cwd','timeout_seconds',
      'inherit_env','output_roles','side_effects'
    ],label);
    const id=identifier(item.id,label+'.id');
    if(commands.has(id)) throw new Error('duplicate command ID: '+id);
    const moduleIds=arrayOfStrings(item.module_ids,label+'.module_ids',{allowEmpty:false});
    moduleIds.forEach((v,i)=>{
      identifier(v,`${label}.module_ids[${i}]`);
      if(!modules.has(v)) throw new Error(`${label} references unknown module: ${v}`);
    });
    if(!CAPABILITIES.has(item.capability)) throw new Error(label+' has an unknown capability');
    const argv=arrayOfStrings(item.argv,label+'.argv',{allowEmpty:false});
    commandExecutable(argv[0],label+'.argv[0]');
    argv.slice(1).forEach((v,i)=>nonempty(v,`${label}.argv[${i+1}]`));
    const base=path.basename(argv[0]).toLowerCase();
    if(base==='env') throw new Error(label+' cannot dispatch through env');
    if(argv.some((value,index)=>index>0&&(SECRET_ARG.test(value)||CREDENTIAL_OPTIONS.has(value)||CREDENTIAL_OPTIONS.has(value.toLowerCase()))))
      throw new Error(label+' contains a credential-like argv value');
    if(['sh','bash','dash','zsh','ksh'].includes(base) &&
       argv.some((v,i)=>i>0 && (/^-[^-]*c/.test(v.toLowerCase())||v.toLowerCase()==='--command')) ||
       ['cmd','powershell','pwsh'].includes(base) &&
       argv.some((v,i)=>i>0 && ['/c','-c','-command'].includes(v.toLowerCase())))
      throw new Error(label+' cannot hide a workflow in a shell string');
    const inlineInterpreter=
      /^python(?:\d+(?:\.\d+)*)?$/.test(base)&&argv.slice(1).some(v=>/^-[^-]*c/.test(v)) ||
      base==='node'&&argv.slice(1).some(v=>/^-[^-]*[ep]/.test(v)||/^--(?:eval|print)(?:=|$)/.test(v)) ||
      base==='perl'&&argv.slice(1).some(v=>/^-[^-]*[eE]/.test(v)) ||
      base==='ruby'&&argv.slice(1).some(v=>/^-[^-]*e/.test(v)) ||
      base==='php'&&argv.slice(1).some(v=>/^-r(?:$|.)/.test(v)) ||
      base==='busybox'&&['sh','ash'].includes((argv[1]||'').toLowerCase());
    if(inlineInterpreter)throw new Error(label+' cannot hide a workflow in an inline interpreter string; use a reviewed repository script');
    const cwd=ensureDirectory(root,item.cwd,label+'.cwd');
    if(!moduleIds.every(moduleId=>moduleContains(modules.get(moduleId).root,cwd)||moduleContains(cwd,modules.get(moduleId).root)))
      throw new Error(label+'.cwd neither belongs to nor orchestrates every declared module');
    if(!Number.isInteger(item.timeout_seconds) || item.timeout_seconds<1 || item.timeout_seconds>86400)
      throw new Error(label+'.timeout_seconds is out of range');
    const inherit=arrayOfStrings(item.inherit_env,label+'.inherit_env');
    for(const name of inherit){
      if(!/^[A-Z_][A-Z0-9_]*$/.test(name) || SECRET_ENV.test(name) || INJECTION_ENV.has(name) ||
         name==='LANG' || name==='LC_ALL' || name.startsWith('AUTOAI_'))
        throw new Error(label+' contains a forbidden inherited environment name: '+name);
    }
    if(!argv[0].startsWith('./')&&!inherit.includes('PATH'))
      throw new Error(label+' uses a PATH command but does not explicitly inherit PATH');
    const requiredTools=arrayOfStrings(item.required_tools||[],label+'.required_tools');
    for(const [toolIndex,tool] of requiredTools.entries())if(!/^[A-Za-z0-9][A-Za-z0-9._+-]*$/.test(tool)||tool==='env')
      throw new Error(`${label}.required_tools[${toolIndex}] is not a safe PATH executable name`);
    if(requiredTools.length&&!inherit.includes('PATH'))throw new Error(label+' required_tools need PATH in the inherited environment');
    if(argv[0].startsWith('./')){
      const executable=path.join(root,...argv[0].slice(2).split('/')),executableStat=fs.lstatSync(executable);
      if(!executableStat.isFile()||executableStat.isSymbolicLink()||(executableStat.mode&0o111)===0)
        throw new Error(label+' repository command must be a non-symlink executable file');
      const realExecutable=fs.realpathSync(executable);
      if(realExecutable!==root&&!realExecutable.startsWith(root+path.sep))
        throw new Error(label+' repository command resolves outside the repository');
    }
    arrayOfStrings(item.output_roles,label+'.output_roles')
      .forEach((v,i)=>identifier(v,`${label}.output_roles[${i}]`));
    const sideEffects=arrayOfStrings(item.side_effects,label+'.side_effects');
    sideEffects.forEach(v=>{if(!SIDE_EFFECTS.has(v))throw new Error(label+' has unknown side effect: '+v)});
    commands.set(id,{...item,cwd,module_ids:moduleIds,required_tools:requiredTools});
  }

  for(const record of adapterExportRecords){
    const command=commands.get(record.provenance.command_id);
    if(!command||command.capability!=='static-analysis'||command.side_effects.length||!command.module_ids.includes(record.module_id))
      throw new Error(record.label+' adapter export command must be a side-effect-free static-analysis command for the same module');
  }

  for(const module of modules.values()){
    for(const [capability,ids] of Object.entries(module.capabilities)){
      for(const id of ids){
        const command=commands.get(id);
        if(!command || command.capability!==capability || !command.module_ids.includes(module.id))
          throw new Error(`module ${module.id} capability ${capability} has an invalid command reference: ${id}`);
      }
    }
  }

  const identities=new Set(),referencedCommands=new Set(
    [...modules.values()].flatMap(module=>Object.values(module.capabilities).flat())
  );
  for(const [index,item] of profile.toolchain_identity.entries()){
    const label=`toolchain_identity[${index}]`;
    closed(item,['id','module_ids','command_id'],label);
    const id=identifier(item.id,label+'.id');
    if(identities.has(id)) throw new Error('duplicate toolchain identity ID: '+id);
    identities.add(id);
    const moduleIds=arrayOfStrings(item.module_ids,label+'.module_ids',{allowEmpty:false});
    moduleIds.forEach(v=>{if(!modules.has(v))throw new Error(label+' references unknown module: '+v)});
    const command=commands.get(identifier(item.command_id,label+'.command_id'));
    if(!command || command.capability!=='static-analysis' || command.side_effects.length)
      throw new Error(label+' must reference a side-effect-free static-analysis command');
    referencedCommands.add(item.command_id);
    if(moduleIds.some(v=>!command.module_ids.includes(v)))
      throw new Error(label+' command does not cover all identity modules');
  }
  const identityBindings=new Set(profile.toolchain_identity.flatMap(identity=>identity.module_ids.map(moduleId=>moduleId+'\0'+identity.command_id)));
  const adapterExportBindings=new Set(adapterExportRecords.map(record=>record.module_id+'\0'+record.provenance.command_id));
  for(const command of commands.values())for(const moduleId of command.module_ids){
    const module=modules.get(moduleId),capabilityBinding=(module.capabilities[command.capability]||[]).includes(command.id);
    const specialBinding=command.capability==='static-analysis'&&
      (identityBindings.has(moduleId+'\0'+command.id)||adapterExportBindings.has(moduleId+'\0'+command.id));
    if(!capabilityBinding&&!specialBinding)
      throw new Error(`command ${command.id} is not exposed by module ${moduleId} capability or an approved identity/export binding`);
  }
  const identityCoveredModules=new Set(profile.toolchain_identity.flatMap(identity=>identity.module_ids));
  const identityRequiredCapabilities=new Set(['configure','build','test','install','package','consumer','target-run']);
  for(const module of modules.values())if(Object.keys(module.capabilities).some(capability=>identityRequiredCapabilities.has(capability))&&!identityCoveredModules.has(module.id))
    throw new Error('module '+module.id+' exposes executable evidence capabilities without a toolchain identity');
  for(const record of adapterExportRecords)referencedCommands.add(record.provenance.command_id);
  for(const id of commands.keys())if(!referencedCommands.has(id))
    throw new Error('orphan Project Profile command is not exposed by a capability or toolchain identity: '+id);

  const profileSha256=digest(Buffer.from(canonical(profile)));
  return {root,file:path.resolve(file),profile,profile_sha256:profileSha256,modules,commands};
}

function draftFromDetection(file,selectedIndices=null){
  const detection=JSON.parse(fs.readFileSync(file,'utf8'));
  if(!detection || detection.schema_version!==1 || !Array.isArray(detection.candidates))
    throw new Error('invalid project detection document');
  let candidates=detection.candidates;
  if(selectedIndices!==null){
    if(!Array.isArray(selectedIndices)||!selectedIndices.length||selectedIndices.some(x=>!Number.isInteger(x)||x<1||x>candidates.length)||new Set(selectedIndices).size!==selectedIndices.length)
      throw new Error('candidate selection is invalid');
    candidates=selectedIndices.map(index=>candidates[index-1]);
  }else if(candidates.length){
    throw new Error('detected candidates require an explicit selection');
  }
  const seen=new Set();
  const modules=[];
  for(const candidate of candidates){
    if(!candidate || !ADAPTERS.has(candidate.adapter) || typeof candidate.module_root!=='string') continue;
    const key=candidate.adapter+'\0'+candidate.module_root;
    if(seen.has(key)) continue;
    seen.add(key);
    const stem=(candidate.adapter+'-'+(modules.length+1)).replace(/[^a-z0-9-]/g,'-');
    modules.push({
      id:stem,
      root:candidate.module_root,
      adapter:candidate.adapter,
      cpp_standards:['unknown'],
      compilers:['unknown'],
      target_platforms:['unknown'],
      path_roles:{production:[],test:[],example:[],generated:[],vendor:[],build_metadata:[]},
      capabilities:{},
      capability_status:{},
      build_targets:[],
      build_graph_entries:[],
      distribution_surfaces:[]
    });
  }
  if(!modules.length){
    modules.push({
      id:'root',root:'.',adapter:'custom',cpp_standards:['unknown'],compilers:['unknown'],
      target_platforms:['unknown'],
      path_roles:{production:[],test:[],example:[],generated:[],vendor:[],build_metadata:[]},
      capabilities:{},capability_status:{},build_targets:[],build_graph_entries:[],distribution_surfaces:[]
    });
  }
  return {schema_version:1,modules,commands:[],toolchain_identity:[]};
}

function commandIdentity(loaded,command){
  return digest(Buffer.from(canonical({
    profile_sha256:loaded.profile_sha256,
    id:command.id,module_ids:command.module_ids,capability:command.capability,
    argv:command.argv,cwd:command.cwd,timeout_seconds:command.timeout_seconds,
    inherit_env:command.inherit_env,required_tools:command.required_tools,output_roles:command.output_roles,
    side_effects:command.side_effects
  })));
}

module.exports={
  ADAPTERS,ADAPTER_CONTRACT_VERSION,CAPABILITIES,PATH_ROLES,SIDE_EFFECTS,canonical,digest,repoRelative,globRegex,
  ensureRepositoryRoot,safeRepositoryEntry,ensureRepositoryDirectory,digestFile,
  parseJsonStrict,parseProfile,draftFromDetection,commandIdentity
};

if(require.main===module){
  try{
    const args=process.argv.slice(2);
    let root=process.cwd(),file=path.join(root,'.ai-harness','project-profile.json'),json=false,draft=null,selection=null;
    for(let i=0;i<args.length;i++){
      switch(args[i]){
        case '--check': break;
        case '--check-file': file=args[++i]; if(!file)throw new Error('--check-file needs a path'); break;
        case '--root': root=args[++i]; if(!root)throw new Error('--root needs a path'); break;
        case '--json': json=true; break;
        case '--digest': json=false; break;
        case '--draft-from-detection': draft=args[++i]; if(!draft)throw new Error('--draft-from-detection needs a path'); break;
        case '--select': {
          const raw=args[++i];if(!raw||!/^\d+(?:,\d+)*$/.test(raw))throw new Error('--select needs comma-separated one-based candidate indices');
          selection=raw.split(',').map(Number);break;
        }
        default: throw new Error('unknown Project Profile option: '+args[i]);
      }
    }
    if(draft){
      process.stdout.write(JSON.stringify(draftFromDetection(draft,selection),null,2)+'\n');
    }else{
      const loaded=parseProfile(root,file);
      if(args.includes('--digest')) process.stdout.write(loaded.profile_sha256+'\n');
      else if(json) process.stdout.write(JSON.stringify({
        schema_version:1,status:'pass',profile_sha256:loaded.profile_sha256,
        modules:[...loaded.modules.keys()].sort(cmp),
        commands:[...loaded.commands.values()].map(x=>({
          id:x.id,capability:x.capability,module_ids:x.module_ids,required_tools:x.required_tools
        })).sort((a,b)=>cmp(a.id,b.id))
      },null,2)+'\n');
      else process.stdout.write(`[OK] Project Profile ${loaded.profile_sha256} (${loaded.modules.size} modules, ${loaded.commands.size} commands)\n`);
    }
  }catch(error){
    if(process.argv.includes('--json'))process.stdout.write(JSON.stringify({schema_version:1,status:'fail',error:{id:'project-profile-invalid',message:error.message}},null,2)+'\n');
    else console.error('[ERR] '+error.message);
    process.exit(4);
  }
}
