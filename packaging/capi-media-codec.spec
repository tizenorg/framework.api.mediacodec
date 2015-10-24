Name:       capi-media-codec
Summary:    A Media Codec library in Tizen Native API
Version:    0.4.2
Release:    0
Group:      Multimedia/API
License:    Apache-2.0
Source0:    %{name}-%{version}.tar.gz
BuildRequires:  cmake
BuildRequires:  pkgconfig(dlog)
BuildRequires:  pkgconfig(glib-2.0)
BuildRequires:  pkgconfig(mm-common)
BuildRequires:  pkgconfig(capi-base-common)
BuildRequires:  pkgconfig(appcore-efl)
BuildRequires:  pkgconfig(capi-media-tool)
BuildRequires:  pkgconfig(libtbm)
BuildRequires:  pkgconfig(gstreamer-1.0)
BuildRequires:  pkgconfig(gstreamer-plugins-base-1.0)
BuildRequires:  pkgconfig(gstreamer-app-1.0)
BuildRequires:  pkgconfig(capi-system-info)
BuildRequires:  pkgconfig(iniparser)
%if "%{tizen_target_name}" == "Z130H" || "%{?tizen_target_name}" == "Z300H"
#!BuildIgnore:  kernel-headers
BuildConflicts: linux-glibc-devel
BuildRequires:  kernel-headers-tizen-dev
%endif
Requires(post): /sbin/ldconfig
Requires(post): libprivilege-control
Requires(postun): /sbin/ldconfig

%description


%package devel
Summary:  A Media Player library in Tizen Native API (Development)
Group:    TO_BE/FILLED_IN
Requires: %{name} = %{version}-%{release}

%description devel

%prep
%setup -q


%build
%if "%{tizen_target_name}" == "Z130H" || "%{?tizen_target_name}" == "Z300H"
export CFLAGS="$CFLAGS -DTIZEN_PROFILE_LITE"
%endif
%if 0%{?sec_build_binary_debug_enable}
export CFLAGS="$CFLAGS -DTIZEN_DEBUG_ENABLE"
export CXXFLAGS="$CXXFLAGS -DTIZEN_DEBUG_ENABLE"
export FFLAGS="$FFLAGS -DTIZEN_DEBUG_ENABLE"
%endif
%ifarch %{arm}
export CFLAGS="$CFLAGS -DENABLE_FFMPEG_CODEC"
%endif

MAJORVER=`echo %{version} | awk 'BEGIN {FS="."}{print $1}'`
cmake . -DCMAKE_INSTALL_PREFIX=/usr -DFULLVER=%{version} -DMAJORVER=${MAJORVER}


make %{?jobs:-j%jobs}

%install
rm -rf %{buildroot}
mkdir -p %{buildroot}/usr/share/license
mkdir -p %{buildroot}/usr/bin
cp test/media_codec_test %{buildroot}/usr/bin
cp LICENSE.APLv2 %{buildroot}/usr/share/license/%{name}

%make_install

%post
/sbin/ldconfig

%postun -p /sbin/ldconfig


%files
%manifest capi-media-codec.manifest
%{_libdir}/libcapi-media-codec.so.*
%{_datadir}/license/%{name}
/usr/bin/*
#%{_bindir}/*

%files devel
%{_includedir}/media/*.h
%{_libdir}/pkgconfig/*.pc
%{_libdir}/libcapi-media-codec.so


