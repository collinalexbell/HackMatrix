from rich.console import Console
from rich.text import Text
import subprocess

# Define the path to your Bash script
bash_script_path = "/path/to/your/script.sh"


cmd = ""
console = Console()
console.clear()
while cmd != "exit":
    text = Text("npc@matrix:~/$ ")
    text.stylize("bold magenta", 0, 3)
    text.stylize("bold green", 4, 10)
    cmd = console.input(text)
    subprocess.run(cmd, shell=True)
