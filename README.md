# GGXrdMirrorColorSelect

![Screenshot can't be viewed](demo.jpg)

Changes connection tiers (T4, T3, T2, T1 and T0) in Guilty Gear Xrd -REVELATOR- Rev2 version 2211 (works as of 6'th December 2024).  
This affects the listing of 'Friend'/'Player'/'Private' (same thing called different names) lobbies (see screenshot above), the filtered results of those lobbies (same screenshot), and the connection tier icon next to a player's name when inside a 'Friend' lobby (see screenshot below)

![Screenshot can't be viewed](demo2.png)

It is unknown whether this affects World Lobbies or Ranked Match - not tested.

The default ping thresholds that the game uses are:

- 0 to 60ms - **T4**;
- 61 to 100ms - **T3**;
- 101 to 200ms - **T2**;
- 201 to 300ms - **T1**;
- \>300ms - **T0**.

## Usage on Windows

Launch GGXrdAdjustConnectionTiers.exe or GGXrdAdjustConnectionTiers_32bit.exe, edit the ping values and press the **Patch GuiltyGear** button.  
Close the mod.  
Launch Guilty Gear Xrd.  
You don't need to ever launch this mod again unless you want to change the ping thresholds.

## Usage on Linux

The launcher provided (launch_GGXrdAdjustConnectionTiers_linux.sh) only works if your Steam on Linux launches Guilty Gear Xrd through Steam Proton.  
Cd into the directory where `GGXrdAdjustConnectionTiers.exe` and `launch_GGXrdAdjustConnectionTiers_linux.sh` files are and give yourself permission to execute the .sh script:

```bash
cd GGXrdAdjustConnectionTiers
chmod u+x launch_GGXrdAdjustConnectionTiers_linux.sh
# Launch Guilty Gear first, then script
./launch_GGXrdAdjustConnectionTiers_linux.sh
```

The .sh script will launch the app's .exe through Wine in the same virtual environment as Guilty Gear Xrd. You may then close Guilty Gear Xrd and press the **Patch GuiltyGear** button.

Steam may start downloading 4GB of stuff and say it's validating something in this game. It will in fact not alter the patched files after it is over, and all the changes will remain.

## Undoing changes if game stopped working

If after pressing the **Patch GuiltyGear** button the game stopped working, you can go into the game installation directory and find and replace the following files:

- Replace `...\Binaries\Win32\GuiltyGearXrd.exe` with `...\Binaries\Win32\GuiltyGearXrd_backup.exe`

This should undo all the changes done by the mod.

## Compilation

To compile, open the .sln file in Visual Studio (I use Visual Studio Community Edition 2022) and press Build - Build Solution. It will say where it compiled the .exe file in the Output panel.

## About

**Guilty Gear** is a registered trademark of ARC SYSTEM WORKS CO., LTD. Me and this code are in no way affiliated with them or being endorsed by them.
