#!/bin/sh

# Create a chroot for cross-building "Let me illustrate...".
#
# Copyright (C) 2016, 2017, 2018, 2019, 2020 Gregory W. Chicares.
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License version 2 as
# published by the Free Software Foundation.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software Foundation,
# Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA
#
# http://savannah.nongnu.org/projects/lmi
# email: <gchicares@sbcglobal.net>
# snail: Chicares, 186 Belle Woods Drive, Glastonbury CT 06033, USA

. ./lmi_setup_inc.sh
. /tmp/schroot_env

set -vx

assert_su
assert_not_chrooted

apt-get update
apt-get --assume-yes install schroot debootstrap

# Download all OS essentials. This step may be done a single time, and
# its tarball used repeatedly. The target ('/tmp/eraseme') directory
# will be created and erased automatically.

if [ ! -e /var/cache/"${CODENAME}"_bootstrap.tar ]; then
  debootstrap --arch=amd64 --make-tarball=/var/cache/"${CODENAME}"_bootstrap.tar \
   "${CODENAME}" /tmp/eraseme
fi
