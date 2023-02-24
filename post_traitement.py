import matplotlib.pyplot as plt


"""
    Step:
    Remove from monitor.txt the information lines
    Clean the result using REGE: \(\d\d\d\d\d\)  Replace All
    Copy/paste the result in a new file.txt
"""

# Fréquence timer : 20 kHz
duration = (387992-64) / 20000


def count(ch):
    count = 0
    for el in channels[ch]:
        if(el != 0):
            #print("Value number:", i,"->", el)
            count+=1
    return count

with open("sort_monitor.txt", "r", encoding="utf-8", errors="ignore") as f:
    values = f.read()
    
values = values.split("\n")
      
print("\nNumber of values:", len(values), "\n")

channels = {"channel0": [], "channel3": [], "channel4": [], "channel5": [], "channel6": [], "channel7": []}
keys = list(channels.keys())


for i, val in enumerate(values):
    try:
        ch, vl = val.split(" ")
    except:
        print("Error:", val, "line", i)

    if ((ch != "7") & (ch != "6") & (ch != "5") & (ch != "4") & (ch != "3") & (ch != "0")):
        print("Error:", ch, "line", i)
    ch = int(ch)
    vl = int(vl, 16)

    if ch == 0:
        channels["channel0"].append(vl)
    elif ch == 3:
        channels["channel3"].append(vl)
    elif ch == 4:
        channels["channel4"].append(vl)
    elif ch == 5:
        channels["channel5"].append(vl)
    elif ch == 6:
        channels["channel6"].append(vl)
    elif ch == 7:
        channels["channel7"].append(vl)
    else:
        print("Error")

for ch in keys:
    try:
        print("Number of values in", ch, ":", len(channels[ch]))
    except:
        pass

print()

for ch in keys:
    try:
        print("Error's ratio", ch, ":", round(100 * count(ch)/len(channels[ch]), 2))
    except:
        pass

print()
print("Duration:", duration, "s")
print("Frequency:", len(values)/duration, "Hz")
print()

for ch in keys:
    print("ADC frequency", ch, ":", len(channels[ch])/duration, "Hz")

fig, axes = plt.subplots(2, 3, figsize=(15, 10))
ax = axes.ravel()

ax[0].plot(channels["channel0"], color="red")
ax[0].set_title("Channel 0")
ax[1].plot(channels["channel3"], color="green")
ax[1].set_title("Channel 3")
ax[2].plot(channels["channel4"], color="blue")
ax[2].set_title("Channel 4")
ax[3].plot(channels["channel5"], color="yellow")
ax[3].set_title("Channel 5")
ax[4].plot(channels["channel6"], color="orange")
ax[4].set_title("Channel 6")
ax[5].plot(channels["channel7"], color="purple")
ax[5].set_title("Channel 7")

plt.show()

print()