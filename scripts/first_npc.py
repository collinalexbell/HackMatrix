# this code is just slop template basically. Asked codex to give me an NPC
# I don't want to edit it right now. I don't want to delete or put it in a gist or something

#!/usr/bin/env python3

"""Simple interactive NPC helper for HackMatrix project onboarding."""

from __future__ import annotations
import subprocess


subprocess.run(["clear"])
PROMPT = "npc@matrix:~/$ "


def print_welcome() -> None:
    print("Hello. I am your first HackMatrix NPC.")
    print("Type 'help' to see topics or 'exit' to leave.\n")


def print_topics() -> None:
    print("Help topics")
    print("1. basic commands")
    print("2. movement and controls (coming soon)")
    print("3. windows and focus (coming soon)")
    print("4. scripting (coming soon)")
    print("5. building and running (coming soon)")
    print()


def show_basic_commands() -> None:
    print("Basic commands")
    print("- `mkdir -p build && cd build && cmake .. && make -j`")
    print("  Build the project.")
    print("- `./launch`")
    print("  Run HackMatrix from the project root.")
    print("- `scripts/install-python-clientlib.sh`")
    print("  Install the Python client library.")
    print("- `python scripts/player-move.py`")
    print("  Run a simple Python client script after installing the client library.")
    print("- `v`")
    print("  Open the app launcher in HackMatrix.")
    print("- `r`")
    print("  Focus the window you are looking at.")
    print("- `Super+e`")
    print("  Leave window focus.")
    print("- `Super+q`")
    print("  Close the focused window.")
    print("- `p`")
    print("  Save a screenshot to `screenshots/`.")
    print("- `esc`")
    print("  Exit HackMatrix when no app is focused.")
    print()


def handle_command(command: str) -> bool:
    normalized = command.strip().lower()

    if normalized in {"exit", "quit"}:
        print("Goodbye.")
        return False

    if normalized in {"help", "menu", "topics"}:
        print_topics()
        return True

    if normalized in {"1", "basic commands", "basic"}:
        show_basic_commands()
        return True

    if normalized == "":
        return True

    print("Unknown command.")
    print("Type 'help' to see topics or 'exit' to leave.\n")
    return True


def main() -> None:
    print_welcome()
    print_topics()

    running = True
    while running:
        try:
            command = input(PROMPT)
        except EOFError:
            print("\nGoodbye.")
            break

        running = handle_command(command)


if __name__ == "__main__":
    main()
