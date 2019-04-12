# Skale build

##### How to build sChain container?

###### 1) Install requirements

```bash
pip install -r requirements.txt
```

###### 2) Copy new `skaled` executable to `scripts/skale_build/executable` directory

###### 3) Run build script

```bash
BUMP_VERSION=patch/minor/major python build_image.py
```

