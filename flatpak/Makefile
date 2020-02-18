PLUGIN = org.opencpn.OpenCPN.Plugin.oesenc

all: build

build:
	flatpak-builder --repo=repo --force-clean \
	    app $(PLUGIN).yaml

install:
	flatpak install --user --reinstall -y $(CURDIR)/repo $(PLUGIN)

uninstall:
	flatpak uninstall --user $(PLUGIN)
