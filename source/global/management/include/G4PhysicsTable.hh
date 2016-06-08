// This code implementation is the intellectual property of
// the GEANT4 collaboration.
//
// By copying, distributing or modifying the Program (or any work
// based on the Program) you indicate your acceptance of this statement,
// and all its terms.
//
// $Id: G4PhysicsTable.hh,v 1.3 1999/11/16 17:40:42 gcosmo Exp $
// GEANT4 tag $Name: geant4-01-00 $
//
// 
// ------------------------------------------------------------
//      GEANT 4 class header file
//
//	History: first implementation, based on object model of
//	2nd December 1995, G.Cosmo
//
//      Modified 01 March 1996, K. Amako
// ------------------------------------------------------------

#ifndef G4PhysicsTable_h
#define G4PhysicsTable_h 1

#include "g4rw/tpordvec.h"
#include "globals.hh"
#include "G4PhysicsVector.hh"

typedef G4RWTPtrOrderedVector<G4PhysicsVector> G4PhysicsTable;

#endif

