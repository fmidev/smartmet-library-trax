%define DIRNAME trax
%define LIBNAME smartmet-%{DIRNAME}
%define SPECNAME smartmet-library-%{DIRNAME}
Summary: Trax library
Name: %{SPECNAME}
Version: 22.2.10
Release: 1%{?dist}.fmi
License: MIT
Group: Development/Libraries
URL: https://github.com/fmidev/smartmet-library-trax
Source0: %{name}.tar.gz
BuildRoot: %{_tmppath}/%{name}-%{version}-%{release}-buildroot-%(%{__id_u} -n)
BuildRequires: boost169-devel
BuildRequires: gcc-c++
BuildRequires: smartmet-library-macgyver-devel >= 22.1.21
%if %{defined el7}
BuildRequires: devtoolset-7-gcc-c++
#TestRequires: devtoolset-7-gcc-c++
%endif
BuildRequires: make
BuildRequires: rpm-build
BuildRequires: gdal34-devel
BuildRequires: geos310-devel
BuildRequires: fmt-devel
Requires: smartmet-library-macgyver >= 22.1.21
Requires: gdal34
Requires: geos310
Requires: fmt
Provides: %{LIBNAME}
#TestRequires: boost169-devel
#TestRequires: gcc-c++

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
%defattr(0664,root,root,0775)
%{_libdir}/lib%{LIBNAME}.so

%package -n %{SPECNAME}-devel
Summary: Isoband/isoline calculation development files
Provides: %{SPECNAME}-devel
Requires: %{SPECNAME} = %{version}-%{release}
BuildRequires: boost169-devel
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
* Thu Feb 10 2022 Mika Heiskanen <mika.heiskanen@fmi.fi> - 22.2.10-1.fmi
- First release candidate
