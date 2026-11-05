%global debug_package %{nil}

Name:           omasign
Version:        0.1.0
Release:        2%{?dist}
Summary:        Sign PDFs with a digital signature that does not look like a defect
License:        MIT AND OFL-1.1
URL:            https://github.com/navh/omasign
Source0:        %{url}/archive/refs/tags/v%{version}/%{name}-%{version}.tar.gz

BuildRequires:  gcc-c++
BuildRequires:  make
BuildRequires:  qt6-qtbase-devel
BuildRequires:  qt6-qtdeclarative-devel
BuildRequires:  qt6-qtsvg-devel
BuildRequires:  poppler-qt6-devel
BuildRequires:  desktop-file-utils
BuildRequires:  nss-tools
Requires:       qt6-qtdeclarative
Requires:       qt6-qtsvg
Requires:       nss-tools

%description
Sign a PDF by dragging a rectangle over the page. Omasign embeds a
cryptographic signature using a key from the NSS store at ~/.pki/nssdb.

%prep
%autosetup

%build
./bin/build

%check
./bin/test

%install
install -Dm755 build/omasign %{buildroot}%{_bindir}/omasign
install -Dm644 pkgbuild/omasign.svg %{buildroot}%{_datadir}/icons/hicolor/scalable/apps/omasign.svg
desktop-file-install --dir=%{buildroot}%{_datadir}/applications pkgbuild/omasign.desktop

%files
%license LICENSE fonts/OFL.txt fonts/OFL-Noto.txt
%doc README.md
%{_bindir}/omasign
%{_datadir}/applications/omasign.desktop
%{_datadir}/icons/hicolor/scalable/apps/omasign.svg

%changelog
* Thu Nov 05 2026 Amos Hebb <navarre24@gmail.com> - 0.1.0-2
- Rebuild from cleaned source
* Thu Aug 27 2026 Amos Hebb <navarre24@gmail.com> - 0.1.0-1
- First release
