# MagWheel

High-resolution HID wheel

## Hardware

### Components
- Xiao RP2040
- AS5600 magnetic rotary encoder (includes a magnet usually)
- 6806-ZZ ball bearing

## Getting Started

1. Install Zephyr [dependencies](https://docs.zephyrproject.org/latest/develop/getting_started/index.html#install-dependencies) and [west](https://docs.zephyrproject.org/latest/develop/getting_started/index.html#install-dependencies) (steps 1-3)
2. Activate the Zephyr environment:
   ```bash
   source ~/zephyrproject/.venv/bin/activate
   ```
3. Initialize your workspace:
   - If you want to clone & initialize this repo in a new empty workspace directory:
     ```bash
     west init -m https://github.com/not-na/magwheel --mr main .
     ```
   - If you have already cloned this repo into an empty workspace directory:
     ```bash
     west init -l ./magwheel
     ```
4. Update all dependencies:
   ```bash
   west update
   west zephyr-export
   west packages pip --install
   west sdk install
   west blobs fetch
   ```
5. Build your application:
   ```bash
   west build magwheel/applications/main -p always -b nucleo_f302r8
   ```
5. Run Twister tests:
    ```bash
    west twister -v -T . --integration
    ```

6. Build your documentation:
   ```bash
   cd magwheel/docs
   make html
   python3 -m http.server --directory _build_sphinx/html
   ```


Parts of this project, especially the HID code, are based on [opendial](https://github.com/nevelate/opendial).


> *This project was bootstrapped with [cookiecutter-zephyr-downstream](https://github.com/tiacsys/cookiecutter-zephyr-downstream).*
