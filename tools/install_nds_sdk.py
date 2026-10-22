"""Install official devkitPro NDS packages in a user-owned SDK directory.

Reads pacman metadata and verifies package SHA-256 before extraction. Does not
register a pacman installation or change machine-wide environment variables.
"""
import argparse
import hashlib
import io
import json
import os
from pathlib import Path, PurePosixPath
import subprocess
import tarfile
import urllib.request

ROOT = Path(__file__).resolve().parents[1]
CACHE = ROOT / '.tools/downloads/devkitpro'
REPOS = {
    'dkp-windows': 'https://pkg.devkitpro.org/packages/windows/x86_64/',
    'dkp-libs': 'https://pkg.devkitpro.org/packages/',
}


def download(url, target):
    request = urllib.request.Request(url, headers={'User-Agent': 'pacman/6.0.2 (Windows x86_64)'})
    partial = target.with_suffix(target.suffix + '.part')
    with urllib.request.urlopen(request, timeout=60) as response, partial.open('wb') as output:
        while block := response.read(1024 * 1024):
            output.write(block)
    partial.replace(target)


def metadata(data):
    fields = {}
    key = None
    for line in data.decode().splitlines():
        if line.startswith('%') and line.endswith('%'):
            key = line.strip('%')
            fields[key] = []
        elif line and key:
            fields[key].append(line)
    return fields


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--destination', type=Path, default=Path.home() / 'devkitPro')
    parser.add_argument('--catalog-only', action='store_true')
    args = parser.parse_args()
    CACHE.mkdir(parents=True, exist_ok=True)
    packages = {}
    for repo, base in REPOS.items():
        archive = CACHE / (repo + '.db')
        download(base + repo + '.db', archive)
        with tarfile.open(archive) as tar:
            for member in tar:
                if member.name.endswith('/desc'):
                    item = metadata(tar.extractfile(member).read())
                    item['base'] = base
                    packages[item['NAME'][0]] = item
    selected = {name for name, item in packages.items() if 'nds-dev' in item.get('GROUPS', [])}
    if not selected:
        raise RuntimeError('No official nds-dev package group found')
    missing = set()
    queue = list(selected)
    while queue:
        name = queue.pop()
        for dependency in packages[name].get('DEPENDS', []):
            dep = dependency.split('>=')[0].split('<=')[0].split('=')[0].split('>')[0].split('<')[0]
            if dep not in packages:
                missing.add(dep)
            elif dep not in selected:
                selected.add(dep)
                queue.append(dep)
    plan = [{'name': n, 'version': packages[n]['VERSION'][0],
             'filename': packages[n]['FILENAME'][0], 'sha256': packages[n]['SHA256SUM'][0],
             'url': packages[n]['base'] + packages[n]['FILENAME'][0]} for n in sorted(selected)]
    (CACHE / 'plan.json').write_text(json.dumps({'packages': plan, 'external_dependencies': sorted(missing)}, indent=2))
    print(json.dumps({'packages': [(p['name'], p['version']) for p in plan], 'external_dependencies': sorted(missing)}, indent=2), flush=True)
    if args.catalog_only:
        return
    # Runtime DLL dependencies are supplied by Windows or package-contained DLLs.
    # Refuse unresolved package dependencies rather than silently omitting them.
    if missing:
        raise RuntimeError('Unresolved SDK package dependencies: ' + ', '.join(sorted(missing)))
    destination = args.destination.resolve()
    destination.mkdir(parents=True, exist_ok=True)
    for entry in plan:
        archive = CACHE / entry['filename']
        if not archive.exists() or hashlib.sha256(archive.read_bytes()).hexdigest() != entry['sha256']:
            print('Downloading ' + entry['filename'], flush=True)
            download(entry['url'], archive)
        if hashlib.sha256(archive.read_bytes()).hexdigest() != entry['sha256']:
            raise RuntimeError('SHA256 mismatch: ' + entry['filename'])
        # Windows bsdtar handles pacman zstd archives; inspect paths first.
        members = subprocess.check_output(['tar', '-tf', str(archive)], text=True).splitlines()
        for member in members:
            path = PurePosixPath(member)
            if path.is_absolute() or '..' in path.parts:
                raise RuntimeError('Unsafe archive path: ' + member)
        # Extract packages under a private staging root. The SDK's opt/devkitpro
        # subtree is copied to destination; metadata remains in the cache.
        stage = CACHE / 'staging' / entry['name']
        stage.mkdir(parents=True, exist_ok=True)
        subprocess.run(['tar', '-xf', str(archive), '-C', str(stage)], check=True)
        import shutil
        sdk_tree = stage / 'opt/devkitpro'
        if not sdk_tree.is_dir() and entry['name'] not in {'devkit-env', 'devkitARM'}:
            raise RuntimeError('Unexpected SDK package layout: ' + entry['name'])
        if sdk_tree.is_dir():
            shutil.copytree(sdk_tree, destination, dirs_exist_ok=True)
        print('Installed ' + entry['name'], flush=True)
    (destination / 'ssb-nds-install.json').write_text(json.dumps({'source': 'official devkitPro pacman repositories', 'packages': plan}, indent=2))
    print('SDK installed at ' + str(destination), flush=True)


if __name__ == '__main__':
    main()
