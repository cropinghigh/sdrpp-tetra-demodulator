libosmocore - set of Osmocom core libraries
===========================================

This repository contains a set of C-language libraries that form the
core infrastructure of many [Osmocom](https://osmocom.org/) Open Source
Mobile Communications projects.

Historically, a lot of this code was developed as part of the
[OpenBSC](https://osmocom.org/projects/openbsc) project, but which are
of a more generic nature and thus useful to (at least) other programs
that we develop in the sphere of Free Software / Open Source mobile
communications.

There is no clear scope of it. We simply move all shared code between
the various Osmocom projects in this library to avoid code duplication.

The libosmocore.git repository build multiple libraries:

* **libosmocore** contains some general-purpose functions like select-loop
  abstraction, message buffers, timers, linked lists
* **libosmovty** contains routines related to the interactive command-line
  interface called VTY
* **libosmogsm** contains definitions and helper code related to GSM protocols
* **libosmoctrl** contains a shared implementation of the Osmocom control
  interface
* **libosmogb** contains an implementation of the Gb interface with its
  NS/BSSGP protocols
* **libosmocodec** contains an implementation of GSM voice codecs
* **libosmocoding** contains an implementation of GSM channel coding
* **libosmosim** contains infrastructure to interface SIM/UICC/USIM cards


Homepage
--------

The official homepage of the project is
<https://osmocom.org/projects/libosmocore/wiki/Libosmocore>

GIT Repository
--------------

You can clone from the official libosmocore.git repository using

	git clone https://gitea.osmocom.org/osmocom/libosmocore

There is a web interface at <https://gitea.osmocom.org/osmocom/libosmocore>

Documentation
-------------

Doxygen-generated API documentation is generated during the build
process, but also available online for each of the sub-libraries at
<https://ftp.osmocom.org/api/latest/libosmocore/>

Mailing List
------------

Discussions related to libosmocore are happening on the
openbsc@lists.osmocom.org mailing list, please see
<https://lists.osmocom.org/mailman/listinfo/openbsc> for subscription
options and the list archive.

Please observe the [Osmocom Mailing List
Rules](https://osmocom.org/projects/cellular-infrastructure/wiki/Mailing_List_Rules)
when posting.

Contributing
------------

Our coding standards are described at
<https://osmocom.org/projects/cellular-infrastructure/wiki/Coding_standards>

We us a gerrit based patch submission/review process for managing
contributions.  Please see
<https://osmocom.org/projects/cellular-infrastructure/wiki/Gerrit> for
more details

The current patch queue for libosmocore can be seen at
<https://gerrit.osmocom.org/#/q/project:libosmocore+status:open>
