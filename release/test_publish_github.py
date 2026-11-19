import sys, unittest
from pathlib import Path
sys.path.insert(0, str(Path(__file__).parent))
import publish_github as p


class Guard(unittest.TestCase):
    def test_refuses_private_repo(self):
        for bad in ("AhmxdNYC/nioh3-engine", "someone/Nioh-3-Engine", ""):
            with self.assertRaises(SystemExit):
                p.check_repo(bad)

    def test_accepts_public_repo(self):
        self.assertEqual(p.check_repo("ahmxdnyc/nioh-3-engine"), p.PUBLIC_REPO)

    def test_private_repo_refused_before_any_gh_call(self):
        with self.assertRaises(SystemExit) as e:
            p.main(["--tag", "v0", "--notes", "x", "--dir", "nowhere", "--repo", "AhmxdNYC/nioh3-engine"])
        self.assertIn("REFUSED", str(e.exception))


if __name__ == "__main__":
    unittest.main()
