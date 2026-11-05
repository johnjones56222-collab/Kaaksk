# Strata, ready to play

This folder is the browser version of Strata (WebAssembly + WebGL 2), built with `tools/build_web.sh` from the
source in this repository, so the game can be played without installing a compiler. It runs in Chrome, Edge,
Firefox and Safari on phones, tablets and computers. See the main README for the touch controls.

## On an Android phone (Termux)

1. Install [Termux](https://termux.dev) (from F-Droid or its GitHub releases).
2. In Termux, run:

   ```sh
   curl -fsSLO https://raw.githubusercontent.com/johnjones56222-collab/Kaaksk/ccr-982b29e2-i2aaie/strata/tools/play_on_android.sh
   bash play_on_android.sh
   ```

   The game opens in Chrome at `http://localhost:8080`. Run the script again any time to update or restart the game;
   `bash play_on_android.sh --stop` stops it.
3. For a fullscreen app icon, open Chrome's menu and choose *Add to home screen* → *Install*. Once loaded, the game
   also starts from the icon while the Termux server is not running (the browser keeps a copy).

Worlds are saved in the browser for `http://localhost:8080`; clearing Chrome's site data deletes them.

## On a computer

```sh
python3 tools/serve_web.py play
```

Then open <http://localhost:8080>. The folder works on any static web host as well; the page picks the
multi-threaded build (`strata-mt.*`) when the site is cross-origin isolated, otherwise the single-threaded one.

## Updating this folder

After changing the source:

```sh
tools/build_web.sh && rm -rf play/*.js play/*.wasm && cp build-web/dist/* play/
```
