"""Record a source comparison of the motion subset with BattleShip's pinned fork."""
import hashlib
import json
import subprocess
from pathlib import Path
from verify_motion import function, REVISION

ROOT = Path(__file__).resolve().parents[1]
ORIGINAL = ROOT / 'upstream/ssb-decomp-re'
BATTLESHIP = ROOT / 'upstream/BattleShip'
FORK = BATTLESHIP / 'decomp'
BATTLESHIP_REV = 'aeb8779bad36d76d949745dc3c2e3909be214f41'
FORK_REV = 'df3170da4932ae5702db007625a2145bd9ce026f'
FUNCTIONS = {
    'src/ft/ftphysics.c': [
        'ftPhysicsClampGroundVel', 'ftPhysicsSetGroundVelFriction',
        'ftPhysicsSetGroundVelAbsStickRange', 'ftPhysicsApplyGravityClampTVel',
        'ftPhysicsClampAirVelXStickRange', 'ftPhysicsApplyAirVelXFriction'],
    'src/ft/ftcommon/ftcommonjump.c': ['ftCommonJumpGetJumpForceButton'],
}


def git(repo, *args):
    return subprocess.check_output(['git', '-C', str(repo), *args], text=True).strip()


def main():
    for repo, expected in [(ORIGINAL, REVISION), (BATTLESHIP, BATTLESHIP_REV), (FORK, FORK_REV)]:
        actual = git(repo, 'rev-parse', 'HEAD')
        if actual != expected:
            raise RuntimeError(f'Unexpected revision in {repo}: {actual}')
    gitlink = git(BATTLESHIP, 'ls-tree', 'HEAD', 'decomp').split()[2]
    if gitlink != FORK_REV:
        raise RuntimeError('Fork revision does not match BattleShip gitlink')
    entries = []
    for path, names in FUNCTIONS.items():
        original = git(ORIGINAL, 'show', f'HEAD:{path}')
        fork = git(FORK, 'show', f'HEAD:{path}')
        for name in names:
            left = function(original, name)
            right = function(fork, name)
            entries.append({'name': name, 'path': path, 'identical_source': left == right,
                            'original_sha256': hashlib.sha256(left.encode()).hexdigest(),
                            'battleship_sha256': hashlib.sha256(right.encode()).hexdigest()})
    result = {'original_revision': REVISION, 'battleship_revision': BATTLESHIP_REV,
              'battleship_decomp_revision': FORK_REV, 'functions': entries,
              'scope': 'Exact function source text in committed trees, not runtime equivalence'}
    output = ROOT / 'docs/battleship-motion-comparison.json'
    output.write_text(json.dumps(result, indent=2) + '\n', encoding='utf-8')
    same = sum(entry['identical_source'] for entry in entries)
    print(f'{same}/{len(entries)} motion function bodies identical in pinned revisions')
    print(output)


if __name__ == '__main__':
    main()
