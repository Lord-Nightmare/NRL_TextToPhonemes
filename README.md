# NRL_TextToPhonemes

This is an archive of various implementations and derivatives of the original NRL English Text to Phoneme algorithm, as developed by Honey Sue Elovitz, Rodney W. Johnson, Astrid McHugh and John E. Shore in 1975.
This algorithm is described in NRL Report 7948 titled "Automatic Translation of English Text to Phonetics by Means of Letter-to-Sound Rules"[1] from January 21, 1976. The algorithm is also described in the IEEE ITASSP article "Letter-to-Sound Rules for Automatic Translation of English Text to Phonetics"[2] from December 1976, as well as later errata from 1977[3].

The NRL Report 7948 contains two SNOBOL4 programs which implement the algorithm: TRANS, which handles the main translation of English Text to Phonetics, and DICT which adds a supplemental dictionary lookup for input words.

This repository contains a typed version of the original TRANS program (intended for SNOBOL4 on a PDP-10) as 'TRANS.SNO' in the 'snobol' sub-directory, as well as a patched version which will run with Mark Emmer/Catspaw's snobol4 'spitbol' (see https://github.com/spitbol) as 'TRANS.SPT'.

In the future, other implementations and derivatives of the NRL algorithm may appear in this repository.

The NRL algorithm or extended derivatives of it was used in many products and places, including but not limited to:
* Don't Ask Software/SoftVoice's Software Automatic Mouth ("SAM") (as the 'RECITER' program) and later derivatives (MacInTalk, Narrator and SVTTS)
* General Instrument/Microchip's CTS256A text pre-processor accessory chip for the SP0256-AL2 speech synthesizer chip
* Steve Ciarcia's Microvox, AKA Intex Talker
* TI's Terminal Emulator II cartridge for the TI 99/4A + Speech Module
* Votrax Type N' Talk and Personal Speech System


References:

* [1] Elovitz, H. S., Johnson, R. W., McHugh, A., Shore, J. E., Automatic Translation of English Text to Phonetics by Means of Letter-to-Sound Rules (NRL Report 7948). Naval Research Laboratory (NRL). Washington, D. C., 1976. https://apps.dtic.mil/sti/pdfs/ADA021929.pdf
* [2] Elovitz, H. S., Johnson, R. W., McHugh, A., Shore, J. E., Letter-to-Sound Rules for Automatic Translation of English Text to Phonetics. IEEE Transactions on Acoustics, Speech, and Signal Processing ( Volume: 24, Issue: 6, Dec 1976) DOI:10.1109/TASSP.1976.1162873
* [3] Elovitz, H. S., Johnson, R. W., McHugh, A., Shore, J. E., Corrections to "Letter-to-Sound Rules for Automatic Translation of English Text to Phonetics". IEEE Transactions on Acoustics, Speech, and Signal Processing ( Volume: 25, Issue: 2, Apr 1977) DOI:10.1109/TASSP.1977.1162926

Further references not yet indexed:
* McHugh, A. https://apps.dtic.mil/sti/pdfs/ADA031391.pdf - automatic stress/inflection rules
