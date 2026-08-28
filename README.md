# MGSFPSUnlock
This is an experimental mod that allows you to play Metal Gear Solid games at framerates above (and below) 60. This mod is a work in progress so you should expect there to be issues.

## Supported Games

- [x] MGS4
- [x] MGS3
- [ ] MGS2

## How to use

1. Download `MGSFPSUnlock.zip` from the [releases](https://github.com/cipherxof/MGSFPSUnlock/releases) page
2. Extract to your MGS2/MGS3/MGS4 install location (i.e `C:\Program Files (x86)\Steam\steamapps\common\MGS3`)
3. Edit `MGSFPSUnlock.ini` and choose your target framerate.
4. For MGS3, install [MGSHDFix](https://github.com/Lyall/MGSHDFix) and enable Borderless Windowed mode (this shouldn't be required in the future)

## Steam Deck/Linux Additional Instructions
🚩**You do not need to do this if you are using Windows!**
- Open up the game properties in Steam and add `WINEDLLOVERRIDES="wininet,winhttp=n,b" %command%` to the launch options.

## Building

```bash
git clone --recursive https://github.com/cipherxof/MGSFPSUnlock.git
cd MGSFPSUnlock
```

### Windows

Open MGSFPSUnlock.sln in Visual Studio (2022) and build

### Linux

Install `mingw-w64-gcc`

```bash
mkdir build && cd build
cmake ..
make
```
