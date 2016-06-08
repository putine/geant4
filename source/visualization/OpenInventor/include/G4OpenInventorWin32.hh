// This code implementation is the intellectual property of
// the GEANT4 collaboration.
//
// By copying, distributing or modifying the Program (or any work
// based on the Program) you indicate your acceptance of this statement,
// and all its terms.
//
// $Id: G4OpenInventorWin32.hh,v 1.3.8.1 1999/12/07 20:53:35 gunter Exp $
// GEANT4 tag $Name: geant4-01-00 $
//
// OpenInventor graphics system factory.

#if defined (G4VIS_BUILD_OIWIN32_DRIVER) || defined (G4VIS_USE_OIWIN32)

#ifndef G4OPENINVENTORWIN32_HH
#define G4OPENINVENTORWIN32_HH

#include "G4OpenInventor.hh"

class G4OpenInventorWin32: public G4OpenInventor {
public:
  G4OpenInventorWin32 ();
  virtual ~G4OpenInventorWin32 ();
};

#endif

#endif