# Privacy policy

*Last updated: 21 September 2026*

Deltos turns photos of documents into clean scans. It is free, open source software
published by Panayotis Katsaloulis. This policy covers the application on every platform and
in every form it is distributed.

## In short

Deltos collects nothing. It has no accounts, no analytics, no telemetry, no advertising and no
update checks. Your photos, the scans made from them and any recognised text stay on your
computer and are never sent to the developer or to anyone else.

## What Deltos works with

- **Your photos and scans.** Detection, perspective correction, enhancement and text
  recognition (OCR) all run on your computer. Nothing is uploaded for processing.
- **Photo metadata.** Deltos reads the focal length and the subject distance stored in a photo
  (EXIF) to work out the true proportions and size of the page. They are used for that
  calculation only and are not kept or sent anywhere.
- **Settings.** Your choices (OCR languages, whether receiving from a phone is on) are stored in
  a small configuration file in your user profile.

## When Deltos uses the network

Deltos touches the network in two cases only, and both start with an action of yours.

**Receive from phone** (off by default). When you turn it on, Deltos accepts photos from
devices on the same local network, either from the LocalSend app or from the upload page it
shows as a link and a QR code. While it is on:

- it listens on a local port (53317 or the next free one) and answers LocalSend discovery
  messages on the local network; to be found it announces a name of the form
  "Deltos (*computer name*)", the name of the operating system and a random identifier that is
  created anew each time the application starts;
- any device on the same network that knows the address can send it an image, so turn it on only
  on networks you trust;
- the transfer is plain, unencrypted HTTP that stays inside your local network; Deltos does not
  contact any server on the internet for it;
- received files are kept in a temporary folder and deleted when the application closes.

Turning it off closes the port immediately.

**Downloading OCR languages.** When you ask for an extra language in the language dialog, Deltos
downloads that language file over HTTPS from the Tesseract project on GitHub
(`github.com/tesseract-ocr/tessdata_best`). As with any download, GitHub sees your IP address;
its handling of that is described in the
[GitHub Privacy Statement](https://docs.github.com/site-policy/privacy-policies/github-general-privacy-statement).
Deltos sends nothing about you or your documents with the request.

## Sharing and disclosure

There is nothing to share: the developer receives no data from Deltos, so none is stored, sold
or disclosed to third parties.

## Your control

Everything Deltos keeps is on your computer. Removing the application, its configuration file
and any downloaded language files removes all of it.

## Children

Deltos is a general utility, is not directed at children and collects no information from anyone.

## Changes

If a future version changes how data is handled, this document will be updated together with
it; its history is public in the project repository.

## Contact

Questions about this policy: <https://github.com/teras/Deltos/issues>
