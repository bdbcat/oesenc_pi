Oesenc flatpak README
---------------------

This is a simple packaging to use the oesenc plugin when using the
opencpn's flatpak package. To build and install:

  - Install flatpak and flatpak-builder as described in https://flatpak.org/
  - Install the opencpn flatpak package. Using the provisionary repo at
    fedorapeople.org do:

      $ flatpak install --user \
          https://leamas.fedorapeople.org/opencpn/opencpn.flatpakref

  - The shipdriver plugin can now be built and installed using

      $ make
      $ make install

  - The oesenc plugin requires extended permissions. Do:

      $ flatpak override --user --allow=devel

After this you could just start opencpn and enable the plugin. You will have to
purchase and register you charts as described on http://o-charts.org/

The actual plugin version built depends on the *commit:* stanza in the yaml file;
update to other versions as preferred. The sources are available at
https://github.com/bdbcat/oesenc_pi.
