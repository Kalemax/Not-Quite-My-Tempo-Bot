from music21 import *
from pathlib import Path #Helps with file paths

#Defines the path to use to access the piece in the same folder as this script
folder = Path(__file__).parent

# Works with .mxl, .musicxml, .xml, and .mid files
score = converter.parse(folder / "farewell_waltz.mid") #Input the piece here after folder

score = score.stripTies()          # A tied note counts once

# msPerBeat = 60000 / BPM <<< Do this calculation later in the code for the esp32 so you can set a custom BPM easily


# Regardless of time signature, we will use quarter note beats as the unit of time

# Measures
measures = score.parts[0].getElementsByClass(stream.Measure)
numMeasures = len(measures) # Holds the total number of measures in the piece
measureStartingBeats = [float(m.offset) for m in measures]   # Lists the starting offset of each measure in quarter note beats (e.g. 0, 4, 8... for a piece in 4/4)

beatsPerMeasure = measureStartingBeats[2] - measureStartingBeats[1]   # Holds how many beats are in a measure (useful for the count off)

onsets = set()
for n in score.flatten().notes:    # Notes and chords from all parts (doesn't include rests), absolute offsets
    if n.duration.isGrace:         # Grace notes have no duration
        continue
    onsets.add(round(float(n.offset), 4))    # Assigns each note an offset value in quarter note beats, representing at which beat it occurs (max 4 decimals)
    # Quarter note = +1, Half note = +2, Eight note = +0.5, etc.

noteOffsets = sorted(onsets) # Sorts the list of offset values in ascending order
# For example, the 5th beat of a piece in 4/4 time would have a value of 4.0, since the offsets start from 0

#Prints all the results
print("Regardless of time signature, we will use quarter note beats as the unit of time")
print("Beat that each note occurs at: " + str(noteOffsets))
print("Beats per measure: " + str(beatsPerMeasure))
print("Total number of measures: " + str(numMeasures))
print("Starting beat of each measure: " + str(measureStartingBeats))

# This saves the info to a file that will be sent to the esp32 to be used in the program
with open(folder / "Project_program" / "data" / "piece.txt", "w") as f:
    f.write(str(beatsPerMeasure) + "\n")
    f.write(str(numMeasures) + "\n")
    f.write(",".join(str(b) for b in measureStartingBeats) + "\n")
    f.write(",".join(str(b) for b in noteOffsets) + "\n")
