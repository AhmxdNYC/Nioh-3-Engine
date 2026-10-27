"""Run the actual packaged executable against its isolated fake release fixture."""
import argparse
from pathlib import Path
import subprocess
def main():
    p = argparse.ArgumentParser()
    p.add_argument('--exe', type=Path, default=Path(__file__).parent / 'dist/Nioh3Updater.exe')
    a = p.parse_args()
    result = subprocess.run([str(a.exe.resolve()), '--self-test'], capture_output=True, text=True, timeout=90)
    print(result.stdout)
    print(result.stderr)
    assert result.returncode == 0, result.returncode
    assert 'PASS: packaged EXE fake 1.7.81' in result.stdout

if __name__ == "__main__":
    main()
