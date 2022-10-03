%define DIRNAME trax
%define LIBNAME smartmet-%{DIRNAME}
%define SPECNAME smartmet-library-%{DIRNAME}

%if 0%{?rhel} && 0%{rhel} < 9
%define smartmet_boost boost169
%else
%define smartmet_boost boost
%endif

Summary: Trax library
Name: %{SPECNAME}
Version: 22.10.3
Release: 2%{?dist}.fmi
License: MIT
Group: Development/Libraries
URL: https://github.com/fmidev/smartmet-library-trax
Source0: %{name}.tar.gz
BuildRoot: %{_tmppath}/%{name}-%{version}-%{release}-buildroot-%(%{__id_u} -n)
BuildRequires: %{smartmet_boost}-devel
BuildRequires: gcc-c++
BuildRequires: smartmet-library-macgyver-devel >= 22.8.23
%if %{defined el7}
BuildRequires: devtoolset-7-gcc-c++
#TestRequires: devtoolset-7-gcc-c++
%endif
BuildRequires: make
BuildRequires: rpm-build
BuildRequires: gdal34-devel
BuildRequires: geos310-devel
BuildRequires: fmt-devel
Requires: smartmet-library-macgyver >= 22.8.23
Requires: gdal34
Requires: geos310
Requires: fmt
Provides: %{LIBNAME}
#TestRequires: %{smartmet_boost}-devel
#TestRequires: gcc-c++
#TestRequires: make
#TestRequires: fmt-devel
#TestRequires: geos310-devel
#TestRequires: gdal34-devel
#TestRequires: smartmet-library-macgyver-devel >= 22.8.23

%description
Isoline/isoband calculation library.

%prep
rm -rf $RPM_BUILD_ROOT

%setup -q -n %{SPECNAME}
 
%build
make %{_smp_mflags}

%install
%makeinstall

%clean
rm -rf $RPM_BUILD_ROOT

%files
%defattr(0775,root,root,0775)
%{_libdir}/lib%{LIBNAME}.so

%package -n %{SPECNAME}-devel
Summary: Isoband/isoline calculation development files
Provides: %{SPECNAME}-devel
Requires: %{SPECNAME} = %{version}-%{release}
BuildRequires: %{smartmet_boost}-devel
BuildRequires: gcc-c++
%if %{defined el7}
BuildRequires: devtoolset-7-gcc-c++
#TestRequires: devtoolset-7-gcc-c++
%endif
BuildRequires: make
BuildRequires: rpm-build

%description -n %{SPECNAME}-devel
Trax isoline/isoband calculation library development files

%files -n %{SPECNAME}-devel
%defattr(0664,root,root,0775)
%{_includedir}/smartmet/%{DIRNAME}

%changelog
* Mon Oct  3 2022 Mika Heiskanen <mika.heiskanen@fmi.fi> - 22.10.3-2.fmi
- Fixed bug in isoband calculation for cells with one missing value

* Mon Oct  3 2022 Mika Heiskanen <mika.heiskanen@fmi.fi> - 22.10.3-1.fmi
- Fixed an eternal loop when encountering too small holes

* Thu Sep 29 2022 Mika Heiskanen <mika.heiskanen@fmi.fi> - 22.9.29-1.fmi
- Fixed global wrapped data problems
- Enabled signed grid coordinates to fix missing value contouring problems

* Thu Aug  4 2022 Mika Heiskanen <mika.heiskanen@fmi.fi> - 22.8.4-1.fmi
- Bug fix to one 2x2 cell isoband case

* Thu Jun 16 2022 Andris Pavēnis <andris.pavenis@fmi.fi> 22.6.16-1.fmi
- Add support of RHEL9, upgrade to libpqxx-7.7.0 (rhel8+) and fmt-8.1.1

* Tue Jun  7 2022 Mika Heiskanen <mika.heiskanen@fmi.fi> - 22.6.7-1.fmi
- More informative error messages on invalid contour limit specs
- Fixed sorting of isoband limits with NaN values

* Thu Jun  2 2022 Mika Heiskanen <mika.heiskanen@fmi.fi> - 22.6.2-1.fmi
- Added Grid::shift method to enable better world wrapped contouring

* Wed May 18 2022 Mika Heiskanen <mika.heiskanen@fmi.fi> - 22.5.18-1.fmi
- Fixed code to use isfinite instead of isnormal

* Mon May 16 2022 Mika Heiskanen <mika.heiskanen@fmi.fi> - 22.5.16-1.fmi
- Added WKB methods needed by the GRIB-server

* Thu May  5 2022 Mika Heiskanen <mika.heiskanen@fmi.fi> - 22.5.5-1.fmi
- Fixed more double vs float rounding issues

* Tue May  3 2022 Mika Heiskanen <mika.heiskanen@fmi.fi> - 22.5.3-1.fmi
- Fixed double -> float rounding issues

* Mon May  2 2022 Mika Heiskanen <mika.heiskanen@fmi.fi> - 22.5.2-1.fmi
- Fixed tiled contour bbox calculations

* Thu Apr 28 2022 Mika Heiskanen <mika.heiskanen@fmi.fi> - 22.4.28-1.fmi
- Fixes to contouring missing value isobands

* Wed Apr 27 2022 Mika Heiskanen <mika.heiskanen@fmi.fi> - 22.4.27-1.fmi
- Added inversion of missing value isobands

* Tue Apr 26 2022 Mika Heiskanen <mika.heiskanen@fmi.fi> - 22.4.26-1.fmi
- Second release candidate

* Fri Feb 11 2022 Mika Heiskanen <mika.heiskanen@fmi.fi> - 22.2.11-1.fmi
- Added automatic sort of given isoline/isoband limits

* Thu Feb 10 2022 Mika Heiskanen <mika.heiskanen@fmi.fi> - 22.2.10-1.fmi
- First release candidate
