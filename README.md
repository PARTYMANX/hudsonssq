# Hudson SSQ Decoder
Reader for Hudson's DDR chart format

Hudson Soft used a custom variant of the SSQ chart format to encode gimmick steps (from this point on referred to as "mush steps". named after "Mush Mode" in Mario Mix) in DDR Mario Mix and Hottest Party.

## Format

Standard SSQ documentation available [here.](https://github.com/SaxxonPike/rhythm-game-formats/blob/master/ddr/ssq.md)

Mush step data is contained in a chunk with the type 9.  The data is mostly a copy of type 3 steps, but has a different scheme for encoding extra data from MAX-era SSQs.  Every section listed here is DWORD aligned.

### Header:

Header is identical to a standard SSQ step chunk, outside of the offset to mush data

```
Offset(h) Type      Length   Description
+00       int32     4        size of chunk in bytes
+04       int16     2        chunk type (in this case, 9)
+06       int16     2        difficulty (same as standard ssq)
+08       int16     2        step count
+0A       int16     2        unused (padding?)
+0C       int32     4        offset to mush and freeze data
```

### Step Offsets

Identical to standard SSQ.

For each step, there is a 32 bit integer offset.

### Steps

Here it gets messy.  Each step is a byte as usual, but for each bit set, there is a byte after the step mask containing an index to a mush item.  For example:

```
03 <- step map, two bits set
00 <- mush index 0; no item
01 <- mush index 1; 1st slot
```

As with standard SSQ, a byte with no bits set corresponds to an entry in the extra data introduced by DDRMAX (freezes).

### Mush and Extra (DDRMAX) Data

This is where all of the Mush data is defined

#### Mush and Extra Data Header

The header for both mush and extra data
```
Offset(h) Type      Length   Description
+00       int32     4        mush and extra data magic number (0x54455845, 'EXET')
+04       int32     4        offset to end of mush and freeze data
```

#### Mush Data

Contains the following header:

```
Offset(h) Type      Length   Description
+00       int32     4        mush data magic number (0x52414843, 'CHAR')
+04       int32     4        offset to end of mush data
```

Followed by entries corresponding to the mush item indices in the step data (hope you were keeping track!  the header doesn't contain the length...).  The entries are in the following format:

```
Offset(h) Type      Length   Description
+00       int32     4        item type index (these are listed later)
+04       int32     4        parameter 1
+08       int32     4        parameter 2
```

Where parameter 1 and 2 are used to change item properties

#### Extra Data

DDRMAX-era extra data for freezes.  Only exists if there is extra data to be read.  Doesn't exist in Mario Mix.

Contains the following header:

```
Offset(h) Type      Length   Description
+00       int32     4        extra data magic number (0x5a455246, 'FREZ')
+04       int32     4        offset to end of extra data
```

Followed by mush SSQ extra data for each 0x00 that occurred in the steps:

```
Offset(h) Type      Length   Description
+00       byte      1        step map (same as steps block)
+04       byte      1        type (usually 01 for freeze)
```

For each step in the step map, there is another byte containing an index in the mush item table.

## Mush Items

The following are the mush item tables for each game:

### Mario Mix

#### 1: Goomba

Decodes as a regular step, appears as a goomba in mush mode.  Moves at regular speed.

#### 2: Koopa Troopa

Decodes as two steps: one at the time of the step, another a quarter note ahead of the step.

#### 3: Bob-omb and Podoboo

Decodes as a regular step

#### 4: Cheep Cheep

Decodes as a regular step.

#### 5: Spiny

Decodes as nothing in normal mode.  Essentially a mine.  Moves at a higher speed than the chart.

#### 6: Blooper

Decodes as a regular step.

#### 7: Hammer

Decodes as a regular step.

#### 8: Coin Switch

Decodes as a regular step.  Turns the steps after it into coins.

#### 9: Boo

Decodes as a regular step.

#### 10: Bob-omb and Podoboo

Decodes as a regular step.  Same as 3?

#### 11: Cheep Cheep

Decodes as a regular step.  Changes its column as it scrolls.

#### 12: Coin Switch

Decodes as a regular step.  Turns the steps after it into coins.  Same as 8?

#### 13: Freezie

Decodes as a regular step.

#### 14: Ice Spiny

Decodes as nothing in normal mode.  Essentially a mine.  Moves at normal speed.

#### 15: Bullet Bill

Decodes as a regular step.

#### 16: Rocket Part

Decodes as a regular step.  Used in Bowser fight.

### Hottest Party

Every item acts identically to Mario Mix, though is skinned to be not-Mario.

## Extracting Hudson Charts

Each Hudson DDR runs on the Mario Party engine to some extent.  All 

### Chart Data

All chart data is contained in data/ssq.bin on the disc.  Use [mpbindump](https://github.com/gamemasterplc/mpbindump) to extract the SSQ files from it.  No file name is associated with the resulting SSQ files.

[mpbinpack](https://github.com/gamemasterplc/mpbinpack) is also available to repack edited SSQ files to be run in-game.

### Music Data

#### Mario Mix

All music is contained in `sound/MRODDR_Snd.msm`.  I haven't found anything that can open or extract this file.

#### Hottest Party

All music is contained in `sound/ddr.brsar`.  This file is able to be opened by [BrawlBox](https://github.com/libertyernie/brawltools).  Inside of the archive, the actual BGM is under `MU/DDR/`.

## TODO
* StepMania output
* Test with Hottest Party 2 and on
* Document Hottest Party series item tables
* Document Hottest Party series unused data and music

