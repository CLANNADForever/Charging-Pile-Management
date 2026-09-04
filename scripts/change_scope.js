#!/usr/bin/env node
'use strict';

const fs = require('fs');
const path = require('path');
const crypto = require('crypto');
const cp = require('child_process');
const { TextDecoder } = require('util');

const utf8 = new TextDecoder('utf-8', { fatal: true });
const sha = value => 'sha256:' + crypto.createHash('sha256').update(value).digest('hex');
const cmp = (a, b) => Buffer.from(a).compare(Buffer.from(b));
const canonical = value => Array.isArray(value)
    ? '[' + value.map(canonical).join(',') + ']'
    : value && typeof value === 'object'
      ? '{' + Object.keys(value).sort(cmp).map(k => JSON.stringify(k) + ':' + canonical(value[k])).join(',') + '}'
      : JSON.stringify(value);

function safePath(value, label = 'repository path') {
    if (typeof value !== 'string' || !value || path.posix.isAbsolute(value) || value.includes('\\') ||
        value.includes('\0') || value.includes('\n') || value.includes('\r')) {
        throw Error(label + ' is unsafe');
    }
    const normalized = path.posix.normalize(value);
    if (normalized !== value || normalized === '..' || normalized.startsWith('../') ||
        normalized === '.git' || normalized.startsWith('.git/')) throw Error(label + ' is unsafe');
    return value;
}

function glob(pattern) {
    safePath(pattern.replace(/[?*]+/g, 'x'), 'classification pattern');
    let out = '^';
    for (let i = 0; i < pattern.length; i++) {
        const c = pattern[i];
        if (c === '*' && pattern[i + 1] === '*') {
            const segmentStart = i === 0 || pattern[i - 1] === '/';
            if (segmentStart && pattern[i + 2] === '/') { out += '(?:.*/)?'; i += 2; }
            else if (segmentStart && i + 2 === pattern.length) { out += '.*'; i++; }
            else { out += '[^/]*'; i++; }
        }
        else if (c === '*') out += '[^/]*';
        else if (c === '?') out += '[^/]';
        else out += '\\.^$+{}()|[]'.includes(c) ? '\\' + c : c;
    }
    return new RegExp(out + '$');
}

function compileClassification(classification) {
    const classes = ['production', 'tests', 'project_docs', 'project_tooling', 'examples', 'generated', 'vendor'];
    if (!classification || typeof classification !== 'object' || Array.isArray(classification) ||
        Object.keys(classification).some(k => !classes.includes(k)) || classes.some(k => !Array.isArray(classification[k]))) {
        throw Error('classification schema mismatch');
    }
    const patterns = {};
    for (const kind of classes) {
        patterns[kind] = classification[kind].map((entry, index) => {
            const pattern = kind === 'generated' ? entry?.output : entry;
            if (typeof pattern !== 'string') throw Error(`classification.${kind}[${index}] is invalid`);
            return glob(pattern);
        });
    }
    return { classes, patterns };
}

const contractPathSyntax = value =>
    /(^|\/)(?:include|public|export)(\/|$)/.test(value) ||
    /\.(?:h|hh|hpp|hxx)$/.test(value);

function compileContractPathPolicy(classification) {
    const { classes, patterns } = compileClassification(classification);
    return value => {
        safePath(value, 'contract declaration path');
        const matches = classes.filter(kind => patterns[kind].some(pattern => pattern.test(value)));
        if (matches.length !== 1) throw Error('contract declaration path must match exactly one classification: ' + value);
        return matches[0] === 'production' && contractPathSyntax(value);
    };
}

function collectScope(root, implementationBase, classification, structuralSchema = 1) {
    root = path.resolve(root);
    if (![1, 2].includes(structuralSchema)) throw Error('unsupported structural allowance schema');
    if (typeof implementationBase !== 'string' || !/^[0-9a-f]{40,64}$/.test(implementationBase)) {
        throw Error('full implementation base required');
    }
    const git = args => cp.execFileSync('git', args, { cwd: root });
    const gitText = args => cp.execFileSync('git', args, { cwd: root, encoding: 'utf8' }).trim();
    if (gitText(['rev-parse', '--verify', implementationBase + '^{commit}']) !== implementationBase) {
        throw Error('implementation base does not resolve exactly');
    }
    try { git(['merge-base', '--is-ancestor', implementationBase, 'HEAD']); }
    catch { throw Error('implementation base is not an ancestor of HEAD'); }

    const decode = (buffer, label) => {
        try { return utf8.decode(buffer); } catch { throw Error(label + ' is not valid UTF-8'); }
    };
    const nul = (buffer, label) => decode(buffer, label).split('\0').filter(Boolean);
    const archiveArtifact = p => p === 'openspec/changes/archive' || p.startsWith('openspec/changes/archive/');
    const policy = require(path.join(root, 'scripts', 'manifest_policy.js')).loadManifest(root);
    const managed = p => policy.isManaged(p) || archiveArtifact(p);
    const { classes, patterns } = compileClassification(classification);
    const classify = p => {
        safePath(p);
        return classes.filter(kind => patterns[kind].some(re => re.test(p)));
    };

    const logical = [];
    const nameTokens = nul(git(['diff', '--name-status', '-z', '--find-renames', implementationBase, '--']), 'Git name-status');
    for (let i = 0; i < nameTokens.length;) {
        const token = nameTokens[i++];
        const code = token[0];
        if (!['A', 'M', 'D', 'R'].includes(code)) throw Error('unsupported Git change status: ' + token);
        if (code === 'R') {
            const oldPath = nameTokens[i++], newPath = nameTokens[i++];
            if (!oldPath || !newPath) throw Error('incomplete Git rename record');
            safePath(oldPath, 'rename old path'); safePath(newPath, 'rename new path');
            const oldManaged=managed(oldPath),newManaged=managed(newPath);if(oldManaged!==newManaged)throw Error('Git rename crosses the managed/unmanaged ownership boundary');
            if (!oldManaged) logical.push({ path: newPath, old_path: oldPath, change_status: 'renamed' });
        } else {
            const p = nameTokens[i++]; if (!p) throw Error('incomplete Git change record'); safePath(p);
            if (!managed(p)) logical.push({ path: p, old_path: null, change_status: code === 'A' ? 'added' : code === 'D' ? 'deleted' : 'modified' });
        }
    }

    const trackedPaths = new Set(logical.flatMap(x => [x.path, ...(x.old_path ? [x.old_path] : [])]));
    for (const p of nul(git(['ls-files', '--others', '--exclude-standard', '-z']), 'Git untracked path data')) {
        safePath(p, 'untracked path');
        if (managed(p) || trackedPaths.has(p)) continue;
        const st = fs.lstatSync(path.join(root, ...p.split('/')));
        if (!st.isFile() || st.isSymbolicLink()) throw Error('untracked path is not a regular file: ' + p);
        logical.push({ path: p, old_path: null, change_status: 'added' }); trackedPaths.add(p);
    }

    const metricMap = new Map;
    for (const record of nul(git(['diff', '--numstat', '-z', '--no-renames', implementationBase, '--']), 'Git numstat')) {
        const fields = record.split('\t');
        if (fields.length < 3) throw Error('invalid Git numstat record');
        const p = fields.slice(2).join('\t'); safePath(p, 'numstat path');
        if (!managed(p)) metricMap.set(p, { path: p, added: fields[0] === '-' ? null : Number(fields[0]), deleted: fields[1] === '-' ? null : Number(fields[1]) });
    }
    for (const x of logical.filter(x => x.change_status === 'added' && !metricMap.has(x.path))) {
        const file = path.join(root, ...x.path.split('/')), bytes = fs.readFileSync(file);
        let added = null;
        if (!bytes.includes(0)) {
            let text; try { text = utf8.decode(bytes); } catch { text = null; }
            if (text !== null) added = bytes.length === 0 ? 0 : text.split('\n').length - (bytes[bytes.length - 1] === 10 ? 1 : 0);
        }
        metricMap.set(x.path, { path: x.path, added, deleted: 0 });
    }

    const buckets = Object.fromEntries(classes.map(k => [k, []]));
    const unclassifiedPaths = [], overlaps = [];
    for (const metric of [...metricMap.values()].sort((a, b) => cmp(a.path, b.path))) {
        const hits = classify(metric.path);
        if (!hits.length) unclassifiedPaths.push(metric.path);
        else if (hits.length > 1) overlaps.push({ path: metric.path, classes: hits });
        else buckets[hits[0]].push(metric);
    }

    const logicalChanges = logical.map(x => {
        const sides = [x.path, ...(x.old_path ? [x.old_path] : [])], classifications = [...new Set(sides.flatMap(classify))].sort(cmp);
        return { ...x, classifications };
    }).sort((a, b) => cmp((a.old_path || '') + '\0' + a.path, (b.old_path || '') + '\0' + b.path));
    const changedProductionPaths = [...new Set(logicalChanges.flatMap(x => {
        const paths = [];
        if (classify(x.path).includes('production')) paths.push(x.path);
        if (x.old_path && classify(x.old_path).includes('production')) paths.push(x.old_path);
        return paths;
    }))].sort(cmp);
    let genericBuildPatterns = [], genericTargetPatterns = [], distributionPatterns = [];
    if (structuralSchema === 2) {
        const profile = require(path.join(root, 'scripts', 'project_profile_lib.js'))
            .parseProfile(root, path.join(root, '.ai-harness', 'project-profile.json'));
        for (const module of profile.modules.values()) {
            genericBuildPatterns.push(...module.path_roles.build_metadata.map(glob));
            genericTargetPatterns.push(...module.build_targets.map(entry => glob(entry.path)));
            for (const entry of module.build_graph_entries) {
                const targetLike = /(?:^|[-_ ])target(?:$|[-_ ])/i.test(entry.kind);
                (targetLike ? genericTargetPatterns : genericBuildPatterns).push(glob(entry.path));
            }
            distributionPatterns.push(...module.distribution_surfaces.map(entry => glob(entry.path)));
        }
    }
    const matchesAny = (value, patterns) => patterns.some(pattern => pattern.test(value));
    const structuralCandidates = [];
    for (const x of logicalChanges) {
        const sides = x.change_status === 'renamed'
            ? [
                { path: x.old_path, old_path: null, change_status: 'deleted' },
                { path: x.path, old_path: null, change_status: 'added' }
            ]
            : [{ path: x.path, old_path: x.old_path, change_status: x.change_status }];
        for (const side of sides) {
            const p = side.path;
            const inProduction = classify(p).includes('production'), inVendor = classify(p).includes('vendor');
            let kind = null, allowance = null;
            if (inVendor) { kind = 'direct-dependency-candidate'; allowance = 'direct_dependencies'; }
            else if (inProduction && contractPathSyntax(p)) { kind = 'public-contract-candidate'; allowance = 'public_contracts'; }
            else if (structuralSchema === 1 && inProduction && (/(^|\/)CMakeLists\.txt$/.test(p) || /\.cmake$/.test(p))) { kind = 'cmake-target-candidate'; allowance = 'cmake_targets'; }
            else if (structuralSchema === 2 && matchesAny(p, distributionPatterns)) { kind = 'distribution-surface-candidate'; allowance = 'distribution_surfaces'; }
            else if (structuralSchema === 2 && matchesAny(p, genericTargetPatterns)) { kind = 'build-target-candidate'; allowance = 'build_targets'; }
            else if (structuralSchema === 2 && matchesAny(p, genericBuildPatterns)) { kind = 'build-graph-candidate'; allowance = 'build_graph_entries'; }
            if (!kind) continue;
            const id = crypto.createHash('sha256').update(kind + '\0' + (side.old_path || '') + '\0' + p + '\0' + side.change_status).digest('hex').slice(0, 12);
            structuralCandidates.push({ candidate_id: 'candidate-' + id, path: p, kind, allowance_kind: allowance, old_path: side.old_path, change_status: side.change_status });
        }
    }
    structuralCandidates.sort((a, b) => cmp(a.candidate_id, b.candidate_id));
    const result = {
        schema_version: 1,
        implementation_base_commit: implementationBase,
        logical_changes: logicalChanges,
        metric_changes: [...metricMap.values()].sort((a, b) => cmp(a.path, b.path)),
        buckets,
        changed_production_paths: changedProductionPaths,
        structural_candidates: structuralCandidates,
        unclassified_paths: unclassifiedPaths,
        classification_overlaps: overlaps
    };
    result.output_sha256 = sha(Buffer.from(canonical(result)));
    return result;
}

module.exports = { collectScope, compileClassification, compileContractPathPolicy, contractPathSyntax, safePath, canonical, sha };

if (require.main === module) {
    try {
        const [base, designJson] = process.argv.slice(2);
        if (!base || !designJson) throw Error('usage: change_scope.js <implementation-base> <classification-json>');
        const result = collectScope(process.cwd(), base, JSON.parse(designJson));
        process.stdout.write(JSON.stringify(result, null, 2) + '\n');
    } catch (error) {
        console.error('[ERR] ' + error.message);
        process.exit(6);
    }
}
