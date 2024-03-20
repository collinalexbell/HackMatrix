from rich.console import Console
from rich.text import Text

cmd = ""
console = Console()
console.clear()
while cmd != "exit":
    text = Text("npc@matrix:~/$ ")
    text.stylize("bold magenta", 0, 3)
    text.stylize("bold green", 4, 10)
    cmd = console.input(text)
