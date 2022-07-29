#!/bin/bash -x

./RunRace.sh ./bcGNU.exe StartRace1.b
./RunRace.sh ./bcGNU_GMP.exe StartRace1.b
./RunRaceF.sh ./bcFreeBSD.exe StartRace1.b
./RunRace.sh ./bcOpenBSD.exe StartRace1.b
./RunRace.sh ./bcOpenBSD_GMP.exe StartRace1.b

./RunRace.sh ./bcGNU.exe StartRace2.b
./RunRace.sh ./bcGNU_GMP.exe StartRace2.b
./RunRaceF.sh ./bcFreeBSD.exe StartRace2.b
./RunRace.sh ./bcOpenBSD.exe StartRace2.b
./RunRace.sh ./bcOpenBSD_GMP.exe StartRace2.b

./RunRace.sh ./bcGNU.exe StartRace3.b
./RunRace.sh ./bcGNU_GMP.exe StartRace3.b
./RunRaceF.sh ./bcFreeBSD.exe StartRace3.b
./RunRace.sh ./bcOpenBSD.exe StartRace3.b
./RunRace.sh ./bcOpenBSD_GMP.exe StartRace3.b
