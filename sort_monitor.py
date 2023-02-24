with open("sort_monitor.txt", "r", encoding="utf-8", errors="ignore") as f:
    monitor = f.read()

monitor = monitor.split("\n")
res = monitor[:117113]


with open("sort_monitor.txt", "w", encoding="utf-8", errors="ignore") as f:
    for el in res:
        f.write(el + "\n")