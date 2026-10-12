"""Entry point. Without arguments the desktop window opens; with arguments the CLI runs."""

import sys

from anonymizer import offline


def run() -> None:
    offline.enforce()
    # Old macOS versions pass a process serial number when launched from Finder.
    sys.argv = [a for a in sys.argv if not a.startswith("-psn_")]
    if len(sys.argv) > 1:
        from anonymizer.cli import main
        sys.exit(main())
    from anonymizer.gui import main
    main()


if __name__ == "__main__":
    run()
