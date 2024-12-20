# ==========================================================================
#     _   _ _ ____            ____          _____
#    | | | (_)  _ \ ___ _ __ / ___|___  _ _|_   _| __ __ _  ___ ___ _ __
#    | |_| | | |_) / _ \ '__| |   / _ \| '_ \| || '__/ _` |/ __/ _ \ '__|
#    |  _  | |  __/  __/ |  | |__| (_) | | | | || | | (_| | (_|  __/ |
#    |_| |_|_|_|   \___|_|   \____\___/|_| |_|_||_|  \__,_|\___\___|_|
#
#       ---  High-Performance Connectivity Tracer (HiPerConTracer)  ---
#                 https://www.nntb.no/~dreibh/hipercontracer/
# ==========================================================================
#
# High-Performance Connectivity Tracer (HiPerConTracer)
# Copyright (C) 2015-2025 by Thomas Dreibholz
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.
#
# Contact: dreibh@simula.no

#############################################################################
#### LOGO                                                                ####
#############################################################################

SET(LOGO "hipercontracer")

# To check the system's icon sizes available:
# find /usr/share/icons/hicolor/ -maxdepth 1 -type d -printf "%f\n" | grep -E "^[0-9]+" | sort | cut -dx -f1 | sort -n
SET(ICON_LENGTHS
   8
   16
   20
   22
   24
   32
   36
   40
   42
   44
   48
   64
   72
   96
   128
   150
   192
   256
   310
   512
)
SET(LOGO_LENGTH 1920)

IF (WITH_ICONS)
   FOREACH(iconLength IN ITEMS ${ICON_LENGTHS};${LOGO_LENGTH})
      # PNG with fixed width, height as necessary:
      ADD_CUSTOM_COMMAND(OUTPUT  ${CMAKE_CURRENT_BINARY_DIR}/${LOGO}-${iconLength}x.png
                         COMMAND ${GS} -dNOSAFER -dQUIET -dBATCH -dNOPAUSE -dNOPROMPT -dEPSCrop -dAlignToPixels=0 -dGridFitTT=2 -sDEVICE=pngalpha -r${iconLength}x -sOUTPUTFILE="${CMAKE_CURRENT_BINARY_DIR}/${LOGO}-${iconLength}x-tmp.png" "${CMAKE_CURRENT_SOURCE_DIR}/${LOGO}.pdf"
                         COMMAND ${GM} convert "${CMAKE_CURRENT_BINARY_DIR}/${LOGO}-${iconLength}x-tmp.png" -resize ${iconLength}x "${CMAKE_CURRENT_BINARY_DIR}/${LOGO}-${iconLength}x.png"
                         COMMAND rm -f "${CMAKE_CURRENT_BINARY_DIR}/${LOGO}-${iconLength}x.png.tmp")
      ADD_CUSTOM_TARGET(generate_${LOGO}_${iconLength}x_png ALL DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/${LOGO}-${iconLength}x.png)

      # PNG with fixed, same width and height:
      IF(NOT ${iconLength} STREQUAL "1920")
         ADD_CUSTOM_COMMAND(OUTPUT  ${CMAKE_CURRENT_BINARY_DIR}/${LOGO}-${iconLength}x${iconLength}.png
                            COMMAND ${GM} convert ${LOGO}-${iconLength}x.png -resize ${iconLength}x -background none -gravity center -extent ${iconLength}x${iconLength} ${LOGO}-${iconLength}x${iconLength}.png
                            DEPENDS generate_${LOGO}_${iconLength}x_png
                                    ${CMAKE_CURRENT_BINARY_DIR}/${LOGO}-${iconLength}x.png)
         ADD_CUSTOM_TARGET(generate_${LOGO}_${iconLength}x${iconLength}_png ALL DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/${LOGO}-${iconLength}x${iconLength}.png)

         # PNG icon of dimension ${iconLength}x${iconLength}:
         INSTALL(FILES       ${CMAKE_CURRENT_BINARY_DIR}/${LOGO}-${iconLength}x${iconLength}.png
                 DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/icons/hicolor/${iconLength}x${iconLength}/apps
                 RENAME      ${LOGO}.png)
      ENDIF()
   ENDFOREACH()

   # A high-resolution logo PNG file:
   INSTALL(FILES       ${CMAKE_CURRENT_BINARY_DIR}/${LOGO}-${LOGO_LENGTH}x.png
           DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/hipercontracer
           RENAME      ${LOGO}.png)

   # Scalable logo PDF file:
   INSTALL(FILES ${LOGO}.pdf DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/hipercontracer)

   # Scalable SVG icon:
   ADD_CUSTOM_COMMAND(OUTPUT  ${CMAKE_CURRENT_BINARY_DIR}/${LOGO}.svg
                      COMMAND ${PDF2SVG} ${CMAKE_CURRENT_SOURCE_DIR}/${LOGO}.pdf ${LOGO}.svg)
   ADD_CUSTOM_TARGET(generate_${LOGO}_svg ALL DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/${LOGO}.svg)
   INSTALL(FILES       ${CMAKE_CURRENT_BINARY_DIR}/${LOGO}.svg
           DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/icons/hicolor/scalable/apps)
ENDIF()
