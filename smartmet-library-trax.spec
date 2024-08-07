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
Version: 24.7.12
Release: 1%{?dist}.fmi
License: MIT
Group: Development/Libraries
URL: https://github.com/fmidev/smartmet-library-trax
Source0: %{name}.tar.gz
BuildRoot: %{_tmppath}/%{name}-%{version}-%{release}-buildroot-%(%{__id_u} -n)
BuildRequires: %{smartmet_boost}-devel
BuildRequires: gcc-c++
BuildRequires: smartmet-library-macgyver-devel >= 24.8.7
BuildRequires: make
BuildRequires: rpm-build
BuildRequires: gdal38-devel
BuildRequires: geos312-devel
BuildRequires: fmt-devel
BuildRequires: libcurl-devel >= 7.61.0
Requires: smartmet-library-macgyver >= 24.8.7
Requires: gdal38
Requires: geos312
Requires: fmt-libs
Requires: libcurl >= 7.61.0
Provides: %{LIBNAME}
#TestRequires: %{smartmet_boost}-devel
#TestRequires: gcc-c++
#TestRequires: make
#TestRequires: fmt-devel
#TestRequires: geos312-devel
#TestRequires: gdal38-devel
#TestRequires: smartmet-library-macgyver-devel >= 24.8.7

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
* Fri Jul 12 2024 Andris Pavēnis <andris.pavenis@fmi.fi> 24.7.12-1.fmi
- Replace many boost library types with C++ standard library ones

* Fri May 24 2024 Mika Heiskanen <mika.heiskanen@fmi.fi> - 24.5.24-1.fmi
- Speed improvements

* Thu Aug 17 2023 Mika Heiskanen <mika.heiskanen@fmi.fi> - 23.8.17-1.fmi
- Moved Savitzky-Golay smoothing to Contour-engine

* Thu Aug  3 2023 Mika Heiskanen <mika.heiskanen@fmi.fi> - 23.8.3-1.fmi
- Improved handling of rounding errors (BRAINSTORM-2679)

* Fri Jul 28 2023 Andris Pavēnis <andris.pavenis@fmi.fi> 23.7.28-1.fmi
- Repackage due to bulk ABI changes in macgyver/newbase/spine

* Tue Jul 25 2023 Mika Heiskanen <mika.heiskanen@fmi.fi> - 23.7.25-1.fmi
- Changed sliver removal to be off by default for backward compatibility

* Mon Jul 24 2023 Mika Heiskanen <mika.heiskanen@fmi.fi> - 23.7.24-2.fmi
- Silenced compiler warnings

* Mon Jul 24 2023 Mika Heiskanen <mika.heiskanen@fmi.fi> - 23.7.24-1.fmi
- Added sliver removal

* Fri Jul 14 2023 Mika Heiskanen <mika.heiskanen@fmi.fi> - 23.7.14-1.fmi
- Improved error handling and reporting
- Silenced compiler warnings

* Mon Jul 10 2023 Andris Pavēnis <andris.pavenis@fmi.fi> 23.7.10-1.fmi
- Use postgresql 15, gdal 3.5, geos 3.11 and proj-9.0

* Fri Jun  9 2023 Mika Heiskanen <mika.heiskanen@fmi.fi> - 23.6.9-1.fmi
- Enabled calculating the line between missing data and valid data

* Thu Dec 22 2022 Mika Heiskanen <mika.heiskanen@fmi.fi> - 22.12.22-1.fmi
- Fixed several issues reported by CodeChecker

* Mon Dec 19 2022 Mika Heiskanen <mika.heiskanen@fmi.fi> - 22.12.19-1.fmi
- Added a shell size setting to let midpoint interpolation know when not to extrapolate outside the grid

* Mon Nov 28 2022 Mika Heiskanen <mika.heiskanen@fmi.fi> - 22.11.28-1.fmi
- Silenced compiler warnings

* Fri Nov 25 2022 Mika Heiskanen <mika.heiskanen@fmi.fi> - 22.11.25-1.fmi
- Add a second ring building phase to make sure all vertices have been processed

* Tue Nov 22 2022 Mika Heiskanen <mika.heiskanen@fmi.fi> - 22.11.22-2.fmi
- Added logarithmic interpolation suitable for contouring radar data

* Tue Nov 22 2022 Mika Heiskanen <mika.heiskanen@fmi.fi> - 22.11.22-1.fmi
- Use double for coordinates and intersection calculations to fix WMS topology errors due to lack of precision

* Thu Nov 17 2022 Mika Heiskanen <mika.heiskanen@fmi.fi> - 22.11.17-1.fmi
- Fixed polyline::end_angle error handling

* Wed Nov 16 2022 Mika Heiskanen <mika.heiskanen@fmi.fi> - 22.11.16-1.fmi
- Fixed crash bug in isoband building

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
