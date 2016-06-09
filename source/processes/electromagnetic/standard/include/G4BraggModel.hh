//
// ********************************************************************
// * DISCLAIMER                                                       *
// *                                                                  *
// * The following disclaimer summarizes all the specific disclaimers *
// * of contributors to this software. The specific disclaimers,which *
// * govern, are listed with their locations in:                      *
// *   http://cern.ch/geant4/license                                  *
// *                                                                  *
// * Neither the authors of this software system, nor their employing *
// * institutes,nor the agencies providing financial support for this *
// * work  make  any representation or  warranty, express or implied, *
// * regarding  this  software system or assume any liability for its *
// * use.                                                             *
// *                                                                  *
// * This  code  implementation is the  intellectual property  of the *
// * GEANT4 collaboration.                                            *
// * By copying,  distributing  or modifying the Program (or any work *
// * based  on  the Program)  you indicate  your  acceptance of  this *
// * statement, and all its terms.                                    *
// ********************************************************************
//
// $Id: G4BraggModel.hh,v 1.6 2005/05/12 11:06:42 vnivanch Exp $
// GEANT4 tag $Name: geant4-08-00 $
//
// -------------------------------------------------------------------
//
// GEANT4 Class header file
//
//
// File name:     G4BraggModel
//
// Author:        Vladimir Ivanchenko
//
// Creation date: 03.01.2002
//
// Modifications:
// 23-12-02 V.Ivanchenko change interface in order to moveto cut per region
// 24-01-03 Make models region aware (V.Ivanchenko)
// 13-02-03 Add name (V.Ivanchenko)
// 12-11-03 Fix for GenericIons (V.Ivanchenko)
// 11-04-05 Major optimisation of internal interfaces (V.Ivantchenko)
//
//
// Class Description:
//
// Implementation of energy loss and delta-electron production
// by heavy slow charged particles using eveluated data

// -------------------------------------------------------------------
//

#ifndef G4BraggModel_h
#define G4BraggModel_h 1

#include "G4VEmModel.hh"

class G4ParticleChangeForLoss;

class G4BraggModel : public G4VEmModel
{

public:

  G4BraggModel(const G4ParticleDefinition* p = 0, const G4String& nam = "Bragg");

  virtual ~G4BraggModel();

  virtual void Initialise(const G4ParticleDefinition*, const G4DataVector&);

  G4double MinEnergyCut(const G4ParticleDefinition*,
			const G4MaterialCutsCouple*);

  virtual G4double ComputeDEDXPerVolume(const G4Material*,
				const G4ParticleDefinition*,
				G4double kineticEnergy,
				G4double cutEnergy);

  virtual G4double CrossSectionPerVolume(const G4Material*,
				 const G4ParticleDefinition*,
				 G4double kineticEnergy,
				 G4double cutEnergy,
				 G4double maxEnergy);

  virtual std::vector<G4DynamicParticle*>* SampleSecondaries(
                                const G4MaterialCutsCouple*,
                                const G4DynamicParticle*,
                                      G4double tmin,
                                      G4double maxEnergy);

protected:

  G4double MaxSecondaryEnergy(const G4ParticleDefinition*,
			      G4double kinEnergy);

private:

  void SetParticle(const G4ParticleDefinition* p);

  G4bool HasMaterial(const G4Material* material);

  G4double StoppingPower(const G4Material* material,
                               G4double kineticEnergy);

  G4double ElectronicStoppingPower(G4double z,
                                   G4double kineticEnergy) const;
 
  void SetMoleculaNumber(G4int number) {iMolecula = number;};

  G4double DEDX(const G4Material* material, G4double kineticEnergy);

  G4bool MolecIsInZiegler1988(const G4Material* material);

  G4double ChemicalFactor(G4double kineticEnergy, G4double eloss125) const;

  void SetExpStopPower125(G4double value) {expStopPower125 = value;};

  // hide assignment operator
  G4BraggModel & operator=(const  G4BraggModel &right);
  G4BraggModel(const  G4BraggModel&);

  const G4ParticleDefinition* particle;
  G4ParticleDefinition*       theElectron;
  G4ParticleChangeForLoss*    fParticleChange;

  G4double mass;
  G4double spin;
  G4double chargeSquare;
  G4double massRate;
  G4double ratio;
  G4double highKinEnergy;
  G4double lowKinEnergy;
  G4double lowestKinEnergy;
  G4double protonMassAMU;
  G4double theZieglerFactor;
  G4double expStopPower125;        // Experimental Stopping power at 125keV

  G4int    iMolecula;          // index in the molecula's table
  G4bool   isIon;
};

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo....

inline G4double G4BraggModel::MaxSecondaryEnergy(
          const G4ParticleDefinition* pd,
                G4double kinEnergy)
{
  if(pd != particle) SetParticle(pd);
  G4double tau  = kinEnergy/mass;
  G4double tmax = 2.0*electron_mass_c2*tau*(tau + 2.) /
                  (1. + 2.0*(tau + 1.)*ratio + ratio*ratio);
  return tmax;
}

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo....

inline void G4BraggModel::SetParticle(const G4ParticleDefinition* p)
{
  particle = p;
  mass = particle->GetPDGMass();
  spin = particle->GetPDGSpin();
  G4double q = particle->GetPDGCharge()/eplus;
  chargeSquare = q*q;
  massRate     = mass/proton_mass_c2;
  ratio = electron_mass_c2/mass;
}

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo....

#endif