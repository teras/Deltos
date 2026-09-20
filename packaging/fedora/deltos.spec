Name:           deltos
Version:        1.0
Release:        1%{?dist}
Summary:        Turn photos of documents into flat, upright, correctly-sized scans

License:        GPL-3.0-only AND Apache-2.0 AND MIT
URL:            https://github.com/teras/Deltos
Source0:        %{url}/archive/refs/tags/v%{version}.tar.gz#/%{name}-%{version}.tar.gz

BuildRequires:  cmake
BuildRequires:  gcc-c++
BuildRequires:  pkgconfig
BuildRequires:  cmake(Qt6Widgets)
BuildRequires:  cmake(Qt6Concurrent)
BuildRequires:  cmake(Qt6Network)
BuildRequires:  opencv-devel >= 4.7
BuildRequires:  tesseract-devel
BuildRequires:  leptonica-devel
BuildRequires:  libheif-devel
BuildRequires:  desktop-file-utils
BuildRequires:  libappstream-glib

Requires:       tesseract
Requires:       tesseract-langpack-eng
Requires:       tesseract-osd
Recommends:     kf6-kimageformats
Suggests:       tesseract-langpack-ell

%description
Deltos is a desktop document scanner that needs no scanner. Photograph a
document at any angle and it returns a flat, upright page with its real
physical dimensions, ready to export as PDF, PNG or JPG.

The page is found with a neural detector and classic edge detection together,
its true proportions are recovered from the perspective using the focal length
recorded by the camera, and its physical size comes from a reference card
beside it, the camera's depth measurement, or the standard paper size it
matches. Its text can then be recognised, selected and copied.

%prep
%autosetup -n Deltos-%{version}

%build
%cmake -DCMAKE_BUILD_TYPE=Release
%cmake_build

%install
%cmake_install

%check
desktop-file-validate %{buildroot}%{_datadir}/applications/onl.ycode.Deltos.desktop
appstream-util validate-relax --nonet %{buildroot}%{_metainfodir}/onl.ycode.Deltos.metainfo.xml

%files
%license LICENSE
%doc README.md
%{_bindir}/deltos
%{_mandir}/man1/deltos.1*
%{_datadir}/deltos/
%{_datadir}/applications/onl.ycode.Deltos.desktop
%{_metainfodir}/onl.ycode.Deltos.metainfo.xml
%{_datadir}/icons/hicolor/*/apps/onl.ycode.Deltos.*

%changelog
* Sun Sep 20 2026 Panayotis Katsaloulis <panayotis@panayotis.com> - 1.0-1
- Initial release.
