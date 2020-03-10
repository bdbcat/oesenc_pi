Oesenc Continous Integration README
-----------------------------------

This document is designed to be generic and terse. More complete 
documentation is available at the Opencpn Developer Manual [[6]](#fn6).

This software uses a continous integration (CI) setup which builds and
deploys artifacts which can be used by the new plugin installer. The
setup is based on storing the sources on github.

In a first step, the software is built. This step uses builders from
appveyor[[1]](#fn1), circleci[[2]](#fn2) and travis[[3]](#fn3). It
produces an xml metadata file and a tarball for each build.

In next step the build artifacts are uploaded to a cloudsmith [[4]](#fn4)
deployment server.

All steps could be configured after cloning the oesenc\_pi sources.

Builders
--------

The appveyor builder can be activated after registering a free account
on the appveyor website. After creating the account, start following the 
project. From this point a new build is initiated as soon as a commit is
pushed to github. Configuration lives in appveyor.yml

The circleci and travis builders works the same way: register an account
and start following the project. The builds are triggered after a commit
is pushed to github. Configuration lives in .circleci/config.yml and
.travis.yml

Circleci requires user to manually request a MacOS builder before it can
be used. This process has been working flawlessly.


Cloudsmith
----------
To get the built artifacts pushed to cloudsmith after build (assuming all
builders are up & running):

  - Register a free account on cloudsmith.
  - Create two open-source repositories called for example 
    *opencpn-plugins-stable* and *opencpn-plugins-unstable*
  - In the builders, set up the following environment variables:

     - **CLOUDSMITH_API_KEY**: The value of the API key for the cloudsmith
       account (available in the cloudsmith ui).
     - **CLOUDSMITH_UNSTABLE_REPO**: The name of the repo for unstable
       (untagged) commits. My is *alec-leamas/opencpn-plugins-unstable*
     - **CLOUDSMITH_STABLE_REPO**: The name of the repo for stable (tagged)
       commits.

After these steps, the artifacts built should start to show up in cloudsmith
when built. The builder logs usually reveals possible errors.

The setup is complete when cloudsmith repos contains tarball(s) and metadata 
file(s). It is essential to check that the url in the metadata file matches
the actual url to the tarball.


Using the generated artifacts
-----------------------------

To use the generated tarballs a new ocpn-plugins.xml file needs to be
genererated. This is ot ouf scope for this document, see the opencpn
plugins project [[5]](#fn5).

<div id="fn1"/> [1] https://www.appveyor.com/ <br>
<div id="fn2"/> [2] https://circleci.com/ <br>
<div id="fn3"/> [3] https://travis-ci.com/ <br>
<div id="fn4"/> [4] https://cloudsmith.io/ <br>
<div id="fn5"/> [5] https://github.com/opencpn/plugins <br>
<div id="fn6"/> [6] https://opencpn.org/wiki/dokuwiki/doku.php?id=opencpn:developer_manual <br>

