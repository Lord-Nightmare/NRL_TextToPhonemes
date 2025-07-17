# NRL_TextToPhonemes

This is an archive of various implementations and derivatives of the original NRL English Text to Phoneme algorithm, as developed by Honey Sue Elovitz, Rodney W. Johnson, Astrid McHugh and John E. Shore in 1975.
This algorithm is described in NRL Report 7948 titled "Automatic Translation of English Text to Phonetics by Means of Letter-to-Sound Rules"[1] from January 21, 1976. The algorithm is also described in the IEEE ITASSP article "Letter-to-Sound Rules for Automatic Translation of English Text to Phonetics"[2] from December 1976, as well as later errata from 1977[3].

The NRL Report 7948 contains two SNOBOL4 programs which implement the algorithm: TRANS, which handles the main translation of English Text to Phonetics, and DICT which adds a supplemental dictionary lookup for input words.

This repository contains a typed version of the original TRANS program (intended for SNOBOL4 on a PDP-10) as 'TRANS.SNO' in the 'snobol' sub-directory, as well as a patched version which will run with Mark Emmer/Catspaw's snobol4 'spitbol' (see https://github.com/spitbol) as 'TRANS.SPT'.

In the future, other implementations and derivatives of the NRL algorithm may appear in this repository.

The NRL algorithm or extended derivatives of it was used in many products and places, including but not limited to:
* Upper Case publishing's ANGLOPHONE package for the CompuTalker CT-1 speech synthesizer
* Street Electronics Inc's EchoII[+] synthesizer package for the Apple II (which is derived from ANGLOPHONE)
* Don't Ask Software/SoftVoice's Software Automatic Mouth ("SAM") (as the 'RECITER' program) for the Atari 800, AppleII, and C64
* SoftVoice's later SAM derivatives (MacInTalk on the Macintosh, Narrator/Translator on the Amiga, and SVTTS on PC)
* General Instrument/Microchip's CTS256A text pre-processor accessory chip for the SP0256-AL2 speech synthesizer chip
* Steve Ciarcia's Microvox, AKA Intex Talker
* Alpha Products' VS100 Speech Synthesizer for the TRS-80
* TI's Terminal Emulator II cartridge for the TI 99/4A + Speech Module
* Votrax Type N' Talk
* Votrax Personal Speech System
* J. C. Broihier and M. J. Crowley's BASIC Plus IV port of the original SNOBOL implementation [4]
* L. Robert Morris' FORTRAN implementation, DECUS package 110375(11-375) [5][6][7]
* DECUS 357015 for RSX-11, may be a later derivative of L. Robert Morris' version above, intended for the Votrax VSK module. [8]


References:

* [1] Elovitz, H. S., Johnson, R. W., McHugh, A., Shore, J. E., "Automatic Translation of English Text to Phonetics by Means of Letter-to-Sound Rules" (NRL Report 7948). Naval Research Laboratory (NRL). Washington, D. C., 1976. https://apps.dtic.mil/sti/pdfs/ADA021929.pdf
* [2] Elovitz, H. S., Johnson, R. W., McHugh, A., Shore, J. E., "Letter-to-Sound Rules for Automatic Translation of English Text to Phonetics". IEEE Transactions on Acoustics, Speech, and Signal Processing ( Volume: 24, Issue: 6, Dec 1976) DOI:10.1109/TASSP.1976.1162873
* [3] Elovitz, H. S., Johnson, R. W., McHugh, A., Shore, J. E., 'Corrections to "Letter-to-Sound Rules for Automatic Translation of English Text to Phonetics"'. IEEE Transactions on Acoustics, Speech, and Signal Processing ( Volume: 25, Issue: 2, Apr 1977) DOI:10.1109/TASSP.1977.1162926
* [4] J. C. Broihier and M. J. Crowley, "A RSTS/E audio response application", DECUS Proceedings, pp. 1355-1375, 1977-May. https://www.computerhistory.org/collections/catalog/102714012
* [5] Morris, L. Robert, "A Fast FORTRAN implementation of the U. S. Naval Research Laboratory Algorithm for Automatic Translation of English Text to Votrax Parameters". ICASSP '79. IEEE International Conference on Acoustics, Speech, and Signal Processing
* [6] https://www.ibiblio.org/pub/academic/computer-science/history/pdp-11/decus/110375.html
* [7] https://bitsavers.org/bits/DEC/pdp11/floppyimages/minc/rx01/DECUS-11-375-RT-11_NRL_ENGLISH_TXT_TO_VOTRAX_MEDIA_KA.img
* [8] ???TODO???

Further references not yet indexed:
* McHugh, A. https://apps.dtic.mil/sti/pdfs/ADA031391.pdf - automatic stress/inflection rules
