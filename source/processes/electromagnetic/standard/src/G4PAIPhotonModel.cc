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
// File name:     G4PAIPhotonModel.cc
//
// Author: Vladimir.Grichine@cern.ch on base of Vladimir Ivanchenko code
//
// Creation date: 05.10.2003
//
// Modifications:
//

#include "G4Region.hh"
#include "G4PhysicsLogVector.hh"
#include "G4PhysicsFreeVector.hh"
#include "G4PhysicsTable.hh"
#include "G4ProductionCutsTable.hh"
#include "G4MaterialCutsCouple.hh"
#include "G4MaterialTable.hh"
#include "G4SandiaTable.hh"
#include "G4PAIxSection.hh"

#include "G4PAIPhotonModel.hh"
#include "Randomize.hh"
#include "G4Electron.hh"
#include "G4Gamma.hh"
#include "G4Poisson.hh"
#include "G4Step.hh"
#include "G4Material.hh"
#include "G4DynamicParticle.hh"
#include "G4ParticleDefinition.hh"


////////////////////////////////////////////////////////////////////////

G4PAIPhotonModel::G4PAIPhotonModel(const G4ParticleDefinition* p, const G4String& nam)
  : G4VEmModel(nam),G4VEmFluctuationModel(nam),
  fLowestKineticEnergy(10.0*keV),
  fHighestKineticEnergy(100.*TeV),
  fTotBin(200),
  fMeanNumber(20),
  fParticle(0),
  fHighKinEnergy(100.*TeV),
  fLowKinEnergy(2.0*MeV),
  fTwoln10(2.0*log(10.0)),
  fBg2lim(0.0169),
  fTaulim(8.4146e-3)
{
  if(p) SetParticle(p);
  fProtonEnergyVector = new G4PhysicsLogVector(fLowestKineticEnergy,
							   fHighestKineticEnergy,
							   fTotBin);
  fPAItransferTable     = 0;
  fPAIphotonTable       = 0;
  fPAIplasmonTable      = 0;

  fPAIdEdxTable         = 0;
  fSandiaPhotoAbsCof    = 0;
  fdEdxVector           = 0;

  fLambdaVector         = 0;
  fdNdxCutVector        = 0;
  fdNdxCutPhotonVector  = 0;
  fdNdxCutPlasmonVector = 0;
}

////////////////////////////////////////////////////////////////////////////

G4PAIPhotonModel::~G4PAIPhotonModel()
{
  if(fProtonEnergyVector) delete fProtonEnergyVector;
  if(fdEdxVector)         delete fdEdxVector ;
  if ( fLambdaVector)     delete fLambdaVector;
  if ( fdNdxCutVector)    delete fdNdxCutVector;

  if( fPAItransferTable )
  {
        fPAItransferTable->clearAndDestroy();
        delete fPAItransferTable ;
  }
  if( fPAIphotonTable )
  {
        fPAIphotonTable->clearAndDestroy();
        delete fPAIphotonTable ;
  }
  if( fPAIplasmonTable )
  {
        fPAIplasmonTable->clearAndDestroy();
        delete fPAIplasmonTable ;
  }
  if(fSandiaPhotoAbsCof)
  {
    for(G4int i=0;i<fSandiaIntervalNumber;i++)
    {
        delete[] fSandiaPhotoAbsCof[i];
    }
    delete[] fSandiaPhotoAbsCof;
  }
}

///////////////////////////////////////////////////////////////////////////////

void G4PAIPhotonModel::SetParticle(const G4ParticleDefinition* p)
{
  fParticle = p;
  fMass = fParticle->GetPDGMass();
  fSpin = fParticle->GetPDGSpin();
  G4double q = fParticle->GetPDGCharge()/eplus;
  fChargeSquare = q*q;
  fLowKinEnergy *= fMass/proton_mass_c2;
  fRatio = electron_mass_c2/fMass;
  fQc = fMass/fRatio;
}

//////////////////////////////////////////////////////////////////////////////

G4double G4PAIPhotonModel::HighEnergyLimit(const G4ParticleDefinition* p)
{
  if(!fParticle) SetParticle(p);
  return fHighKinEnergy;
}

///////////////////////////////////////////////////////////////////////////

G4double G4PAIPhotonModel::LowEnergyLimit( const G4ParticleDefinition* p )
{
  if(!fParticle) SetParticle(p);
  return fLowKinEnergy;
}

////////////////////////////////////////////////////////////////////////////

G4double G4PAIPhotonModel::MinEnergyCut( const G4ParticleDefinition*,
                                   const G4MaterialCutsCouple*)
{
  //  return couple->GetMaterial()->GetIonisation()->GetMeanExcitationEnergy();
  return 0.0; // any positive cut
}

////////////////////////////////////////////////////////////////////////////

G4bool G4PAIPhotonModel::IsInCharge( const G4ParticleDefinition* p )
{
  if(!fParticle) SetParticle(p);
  return (p->GetPDGCharge() != 0.0 );
}

////////////////////////////////////////////////////////////////////////////

void G4PAIPhotonModel::Initialise(const G4ParticleDefinition* p,
                                   const G4DataVector&)
{
  if(!fParticle) SetParticle(p);

  const G4ProductionCutsTable* theCoupleTable =
        G4ProductionCutsTable::GetProductionCutsTable();

  for(size_t iReg = 0; iReg < fPAIRegionVector.size();++iReg) // region loop
  {
    const G4Region* curReg = fPAIRegionVector[iReg];

    // (*fPAIRegionVector[iRegion])

    std::vector<G4Material*>::const_iterator matIter = curReg->GetMaterialIterator();
    size_t jMat; 
    size_t numOfMat = curReg->GetNumberOfMaterials();

    //  for(size_t jMat = 0; jMat < curReg->GetNumberOfMaterials();++jMat){}
    const G4MaterialTable* theMaterialTable = G4Material::GetMaterialTable();
    size_t numberOfMat = G4Material::GetNumberOfMaterials();

    for(jMat = 0 ; jMat < numOfMat; ++jMat) // region material loop
    {
      const G4MaterialCutsCouple* matCouple = theCoupleTable->
      GetMaterialCutsCouple( *matIter, curReg->GetProductionCuts() );
      fMaterialCutsCoupleVector.push_back(matCouple);

      size_t iMatGlob;
      for(iMatGlob = 0 ; iMatGlob < numberOfMat ; iMatGlob++ )
      {
        if( *matIter == (*theMaterialTable)[iMatGlob]) break ;
      }
      fMatIndex = iMatGlob;

      ComputeSandiaPhotoAbsCof();
      BuildPAIonisationTable();

      fPAIxscBank.push_back(fPAItransferTable);
      fPAIphotonBank.push_back(fPAIphotonTable);
      fPAIplasmonBank.push_back(fPAIplasmonTable);
      fPAIdEdxBank.push_back(fPAIdEdxTable);
      fdEdxTable.push_back(fdEdxVector);

      BuildLambdaVector(matCouple);

      fdNdxCutTable.push_back(fdNdxCutVector);
      fdNdxCutPhotonTable.push_back(fdNdxCutPhotonVector);
      fdNdxCutPlasmonTable.push_back(fdNdxCutPlasmonVector);
      fLambdaTable.push_back(fLambdaVector);


      matIter++;
    }
  }
}

//////////////////////////////////////////////////////////////////

void G4PAIPhotonModel::ComputeSandiaPhotoAbsCof()
{
  G4int i, j, numberOfElements ;
  static const G4MaterialTable* theMaterialTable = G4Material::GetMaterialTable();

  G4SandiaTable thisMaterialSandiaTable(fMatIndex) ;
  numberOfElements = (*theMaterialTable)[fMatIndex]->
                                              GetNumberOfElements();
  G4int* thisMaterialZ = new G4int[numberOfElements] ;

  for(i=0;i<numberOfElements;i++)  
  {
    thisMaterialZ[i] = 
    (G4int)(*theMaterialTable)[fMatIndex]->GetElement(i)->GetZ() ;
  }  
  fSandiaIntervalNumber = thisMaterialSandiaTable.SandiaIntervals
                           (thisMaterialZ,numberOfElements) ;

  fSandiaIntervalNumber = thisMaterialSandiaTable.SandiaMixing
                           ( thisMaterialZ ,
                             (*theMaterialTable)[fMatIndex]->GetFractionVector() ,
        		     numberOfElements,fSandiaIntervalNumber) ;
   
  fSandiaPhotoAbsCof = new G4double*[fSandiaIntervalNumber] ;

  for(i=0;i<fSandiaIntervalNumber;i++)  fSandiaPhotoAbsCof[i] = new G4double[5] ;
   
  for( i = 0 ; i < fSandiaIntervalNumber ; i++ )
  {
    fSandiaPhotoAbsCof[i][0] = thisMaterialSandiaTable.GetPhotoAbsorpCof(i+1,0) ; 

    for( j = 1; j < 5 ; j++ )
    {
      fSandiaPhotoAbsCof[i][j] = thisMaterialSandiaTable.
	                              GetPhotoAbsorpCof(i+1,j)*
                 (*theMaterialTable)[fMatIndex]->GetDensity() ;
    }
  }
  // delete[] thisMaterialZ ;
}

////////////////////////////////////////////////////////////////////////////
//
// Build tables for the ionization energy loss
//  the tables are built for MATERIALS
//                           *********

void
G4PAIPhotonModel::BuildPAIonisationTable()
{
  G4double LowEdgeEnergy , ionloss ;
  G4double massRatio, tau, Tmax, Tmin, Tkin, deltaLow, gamma, bg2 ;
  /*
  if( fPAItransferTable )
  {
     fPAItransferTable->clearAndDestroy() ;
     delete fPAItransferTable ;
  }
  */
  fPAItransferTable = new G4PhysicsTable(fTotBin);
  /*
  if( fPAIratioTable )
  {
     fPAIratioTable->clearAndDestroy() ;
     delete fPAIratioTable ;
  }
  */
  fPAIphotonTable = new G4PhysicsTable(fTotBin);
  fPAIplasmonTable = new G4PhysicsTable(fTotBin);
  /*
  if( fPAIdEdxTable )
  {
     fPAIdEdxTable->clearAndDestroy() ;
     delete fPAIdEdxTable ;
  }
  */
  fPAIdEdxTable = new G4PhysicsTable(fTotBin);

  //  if(fdEdxVector) delete fdEdxVector ;
  fdEdxVector = new G4PhysicsLogVector( fLowestKineticEnergy,
					 fHighestKineticEnergy,
					 fTotBin               ) ;
  Tmin     = fSandiaPhotoAbsCof[0][0] ;      // low energy Sandia interval
  deltaLow = 0.5*eV ;

  for (G4int i = 0 ; i < fTotBin ; i++)  //The loop for the kinetic energy
  {
    LowEdgeEnergy = fProtonEnergyVector->GetLowEdgeEnergy(i) ;
    tau = LowEdgeEnergy/proton_mass_c2 ;
    //    if(tau < 0.01)  tau = 0.01 ;
    gamma = tau +1. ;
    // G4cout<<"gamma = "<<gamma<<endl ;
    bg2 = tau*(tau + 2. ) ;
    massRatio = electron_mass_c2/proton_mass_c2 ;
    Tmax = 2.*electron_mass_c2*bg2/(1.+2.*gamma*massRatio+massRatio*massRatio) ;
    // G4cout<<"proton Tkin = "<<LowEdgeEnergy/MeV<<" MeV"
    // <<" Tmax = "<<Tmax/MeV<<" MeV"<<G4endl;
    // Tkin = DeltaCutInKineticEnergyNow ;

    // if ( DeltaCutInKineticEnergyNow > Tmax)         // was <
    {
      Tkin = Tmax ;
    }
    if ( Tkin < Tmin + deltaLow )  // low energy safety
    {
      Tkin = Tmin + deltaLow ;
    }
    G4PAIxSection protonPAI( fMatIndex,
                             Tkin,
                             bg2,
                             fSandiaPhotoAbsCof,
                             fSandiaIntervalNumber  ) ;


    // G4cout<<"ionloss = "<<ionloss*cm/keV<<" keV/cm"<<endl ;
    // G4cout<<"n1 = "<<protonPAI.GetIntegralPAIxSection(1)*cm<<" 1/cm"<<endl ;
    // G4cout<<"protonPAI.GetSplineSize() = "<<
    //    protonPAI.GetSplineSize()<<G4endl<<G4endl ;

    G4PhysicsFreeVector* transferVector = new
                             G4PhysicsFreeVector(protonPAI.GetSplineSize()) ;
    G4PhysicsFreeVector* photonVector = new
                             G4PhysicsFreeVector(protonPAI.GetSplineSize()) ;
    G4PhysicsFreeVector* plasmonVector = new
                             G4PhysicsFreeVector(protonPAI.GetSplineSize()) ;
    G4PhysicsFreeVector* dEdxVector = new
                             G4PhysicsFreeVector(protonPAI.GetSplineSize()) ;

    for( G4int k = 0 ; k < protonPAI.GetSplineSize() ; k++ )
    {
      transferVector->PutValue( k ,
                                protonPAI.GetSplineEnergy(k+1),
                                protonPAI.GetIntegralPAIxSection(k+1) ) ;
      photonVector->PutValue( k ,
                                protonPAI.GetSplineEnergy(k+1),
                                protonPAI.GetIntegralCerenkov(k+1) ) ;
      plasmonVector->PutValue( k ,
                                protonPAI.GetSplineEnergy(k+1),
                                protonPAI.GetIntegralPlasmon(k+1) ) ;
      dEdxVector->PutValue( k ,
                                protonPAI.GetSplineEnergy(k+1),
                                protonPAI.GetIntegralPAIdEdx(k+1) ) ;
    }
    ionloss = protonPAI.GetMeanEnergyLoss() ;   //  total <dE/dx>
    if ( ionloss <= 0.)  ionloss = DBL_MIN ;
    fdEdxVector->PutValue(i,ionloss) ;

    fPAItransferTable->insertAt(i,transferVector) ;
    fPAIphotonTable->insertAt(i,photonVector) ;
    fPAIplasmonTable->insertAt(i,plasmonVector) ;
    fPAIdEdxTable->insertAt(i,dEdxVector) ;

    // delete[] transferVector ;
    }                                        // end of Tkin loop
   //  theLossTable->insert(fdEdxVector);
                                              // end of material loop
   // G4cout<<"G4PAIonisation::BuildPAIonisationTable() have been called"<<G4endl ;
   // G4cout<<"G4PAIonisation::BuildLossTable() have been called"<<G4endl ;
}

///////////////////////////////////////////////////////////////////////
//
// Build mean free path tables for the delta ray production process
//     tables are built for MATERIALS
//

void
G4PAIPhotonModel::BuildLambdaVector(const G4MaterialCutsCouple* matCutsCouple)
{
  G4int i ;
  G4double dNdxCut,dNdxPhotonCut,dNdxPlasmonCut, lambda;

  const G4ProductionCutsTable* theCoupleTable=
        G4ProductionCutsTable::GetProductionCutsTable();

  size_t numOfCouples = theCoupleTable->GetTableSize();
  size_t jMatCC;

  for (jMatCC = 0 ; jMatCC < numOfCouples ; jMatCC++ )
  {
    if( matCutsCouple == theCoupleTable->GetMaterialCutsCouple(jMatCC) ) break;
  }
  if( jMatCC == numOfCouples && jMatCC > 0 ) jMatCC--;

  const std::vector<G4double>*  deltaCutInKineticEnergy = theCoupleTable->
                                GetEnergyCutsVector(idxG4ElectronCut);
  const std::vector<G4double>*  photonCutInKineticEnergy = theCoupleTable->
                                GetEnergyCutsVector(idxG4GammaCut);

  if (fLambdaVector)         delete fLambdaVector;
  if (fdNdxCutVector)        delete fdNdxCutVector;
  if (fdNdxCutPhotonVector)  delete fdNdxCutPhotonVector;
  if (fdNdxCutPlasmonVector) delete fdNdxCutPlasmonVector;

  fLambdaVector = new G4PhysicsLogVector( fLowestKineticEnergy,
					  fHighestKineticEnergy,
					  fTotBin                );
  fdNdxCutVector = new G4PhysicsLogVector( fLowestKineticEnergy,
					  fHighestKineticEnergy,
					  fTotBin                );
  fdNdxCutPhotonVector = new G4PhysicsLogVector( fLowestKineticEnergy,
					  fHighestKineticEnergy,
					  fTotBin                );
  fdNdxCutPlasmonVector = new G4PhysicsLogVector( fLowestKineticEnergy,
					  fHighestKineticEnergy,
					  fTotBin                );

  G4double deltaCutInKineticEnergyNow  = (*deltaCutInKineticEnergy)[jMatCC];
  G4double photonCutInKineticEnergyNow = (*photonCutInKineticEnergy)[jMatCC];

  G4cout<<"PAIPhotonModel deltaCutInKineticEnergyNow = "
        <<deltaCutInKineticEnergyNow/keV<<" keV"<<G4endl;
  G4cout<<"PAIPhotonModel photonCutInKineticEnergyNow = "
        <<photonCutInKineticEnergyNow/keV<<" keV"<<G4endl;

  for ( i = 0 ; i < fTotBin ; i++ )
  {
    dNdxPhotonCut  = GetdNdxPhotonCut(i,photonCutInKineticEnergyNow);
    dNdxPlasmonCut = GetdNdxPlasmonCut(i,deltaCutInKineticEnergyNow);

    dNdxCut        =  dNdxPhotonCut + dNdxPlasmonCut;
    lambda         = dNdxCut <= DBL_MIN ? DBL_MAX: 1.0/dNdxCut;

    if (lambda <= 1000*kCarTolerance) lambda = 1000*kCarTolerance; // Mmm ???

    fLambdaVector->PutValue(i, lambda);

    fdNdxCutVector->PutValue(i, dNdxCut);
    fdNdxCutPhotonVector->PutValue(i, dNdxPhotonCut);
    fdNdxCutPlasmonVector->PutValue(i, dNdxPlasmonCut);
  }
}

///////////////////////////////////////////////////////////////////////
//
// Returns integral PAI cross section for energy transfers >= transferCut

G4double  
G4PAIPhotonModel::GetdNdxCut( G4int iPlace, G4double transferCut)
{ 
  G4int iTransfer;
  G4double x1, x2, y1, y2, dNdxCut;
  // G4cout<<"iPlace = "<<iPlace<<"; "<<"transferCut = "<<transferCut<<G4endl;
  // G4cout<<"size = "<<G4int((*fPAItransferTable)(iPlace)->GetVectorLength())
  //           <<G4endl;  
  for( iTransfer = 0 ; 
       iTransfer < G4int((*fPAItransferTable)(iPlace)->GetVectorLength()) ; 
       iTransfer++)
  {
    if(transferCut <= (*fPAItransferTable)(iPlace)->GetLowEdgeEnergy(iTransfer))
    {
      break ;
    }
  }  
  if ( iTransfer >= G4int((*fPAItransferTable)(iPlace)->GetVectorLength()) )
  {
      iTransfer = (*fPAItransferTable)(iPlace)->GetVectorLength() - 1 ;
  }
  y1 = (*(*fPAItransferTable)(iPlace))(iTransfer-1) ;
  y2 = (*(*fPAItransferTable)(iPlace))(iTransfer) ;
  // G4cout<<"y1 = "<<y1<<"; "<<"y2 = "<<y2<<G4endl;
  x1 = (*fPAItransferTable)(iPlace)->GetLowEdgeEnergy(iTransfer-1) ;
  x2 = (*fPAItransferTable)(iPlace)->GetLowEdgeEnergy(iTransfer) ;
  // G4cout<<"x1 = "<<x1<<"; "<<"x2 = "<<x2<<G4endl;

  if ( y1 == y2 )    dNdxCut = y2 ;
  else
  {
    //  if ( x1 == x2  ) dNdxCut = y1 + (y2 - y1)*G4UniformRand() ;
    if ( abs(x1-x2) <= eV  ) dNdxCut = y1 + (y2 - y1)*G4UniformRand() ;
    else             dNdxCut = y1 + (transferCut - x1)*(y2 - y1)/(x2 - x1) ;      
  }
  //  G4cout<<""<<dNdxCut<<G4endl;
  return dNdxCut ;
}

///////////////////////////////////////////////////////////////////////
//
// Returns integral PAI cherenkovcross section for energy transfers >= transferCut

G4double  
G4PAIPhotonModel::GetdNdxPhotonCut( G4int iPlace, G4double transferCut)
{ 
  G4int iTransfer;
  G4double x1, x2, y1, y2, dNdxCut;
  // G4cout<<"iPlace = "<<iPlace<<"; "<<"transferCut = "<<transferCut<<G4endl;
  // G4cout<<"size = "<<G4int((*fPAIphotonTable)(iPlace)->GetVectorLength())
  //           <<G4endl;  
  for( iTransfer = 0 ; 
       iTransfer < G4int((*fPAIphotonTable)(iPlace)->GetVectorLength()) ; 
       iTransfer++)
  {
    if(transferCut <= (*fPAIphotonTable)(iPlace)->GetLowEdgeEnergy(iTransfer))
    {
      break ;
    }
  }  
  if ( iTransfer >= G4int((*fPAIphotonTable)(iPlace)->GetVectorLength()) )
  {
      iTransfer = (*fPAIphotonTable)(iPlace)->GetVectorLength() - 1 ;
  }
  y1 = (*(*fPAIphotonTable)(iPlace))(iTransfer-1) ;
  y2 = (*(*fPAIphotonTable)(iPlace))(iTransfer) ;
  // G4cout<<"y1 = "<<y1<<"; "<<"y2 = "<<y2<<G4endl;
  x1 = (*fPAIphotonTable)(iPlace)->GetLowEdgeEnergy(iTransfer-1) ;
  x2 = (*fPAIphotonTable)(iPlace)->GetLowEdgeEnergy(iTransfer) ;
  // G4cout<<"x1 = "<<x1<<"; "<<"x2 = "<<x2<<G4endl;

  if ( y1 == y2 )    dNdxCut = y2 ;
  else
  {
    //  if ( x1 == x2  ) dNdxCut = y1 + (y2 - y1)*G4UniformRand() ;
    if ( abs(x1-x2) <= eV  ) dNdxCut = y1 + (y2 - y1)*G4UniformRand() ;
    else             dNdxCut = y1 + (transferCut - x1)*(y2 - y1)/(x2 - x1) ;      
  }
  //  G4cout<<""<<dNdxPhotonCut<<G4endl;
  return dNdxCut ;
}

///////////////////////////////////////////////////////////////////////
//
// Returns integral PAI cross section for energy transfers >= transferCut

G4double  
G4PAIPhotonModel::GetdNdxPlasmonCut( G4int iPlace, G4double transferCut)
{ 
  G4int iTransfer;
  G4double x1, x2, y1, y2, dNdxCut;

  // G4cout<<"iPlace = "<<iPlace<<"; "<<"transferCut = "<<transferCut<<G4endl;
  // G4cout<<"size = "<<G4int((*fPAIPlasmonTable)(iPlace)->GetVectorLength())
  //           <<G4endl;  
  for( iTransfer = 0 ; 
       iTransfer < G4int((*fPAIplasmonTable)(iPlace)->GetVectorLength()) ; 
       iTransfer++)
  {
    if(transferCut <= (*fPAIplasmonTable)(iPlace)->GetLowEdgeEnergy(iTransfer))
    {
      break ;
    }
  }  
  if ( iTransfer >= G4int((*fPAIplasmonTable)(iPlace)->GetVectorLength()) )
  {
      iTransfer = (*fPAIplasmonTable)(iPlace)->GetVectorLength() - 1 ;
  }
  y1 = (*(*fPAIplasmonTable)(iPlace))(iTransfer-1) ;
  y2 = (*(*fPAIplasmonTable)(iPlace))(iTransfer) ;
  // G4cout<<"y1 = "<<y1<<"; "<<"y2 = "<<y2<<G4endl;
  x1 = (*fPAIplasmonTable)(iPlace)->GetLowEdgeEnergy(iTransfer-1) ;
  x2 = (*fPAIplasmonTable)(iPlace)->GetLowEdgeEnergy(iTransfer) ;
  // G4cout<<"x1 = "<<x1<<"; "<<"x2 = "<<x2<<G4endl;

  if ( y1 == y2 )    dNdxCut = y2 ;
  else
  {
    //  if ( x1 == x2  ) dNdxCut = y1 + (y2 - y1)*G4UniformRand() ;
    if ( abs(x1-x2) <= eV  ) dNdxCut = y1 + (y2 - y1)*G4UniformRand() ;
    else             dNdxCut = y1 + (transferCut - x1)*(y2 - y1)/(x2 - x1) ;      
  }
  //  G4cout<<""<<dNdxPlasmonCut<<G4endl;
  return dNdxCut ;
}

///////////////////////////////////////////////////////////////////////
//
// Returns integral dEdx for energy transfers >= transferCut

G4double  
G4PAIPhotonModel::GetdEdxCut( G4int iPlace, G4double transferCut)
{ 
  G4int iTransfer;
  G4double x1, x2, y1, y2, dEdxCut;
  // G4cout<<"iPlace = "<<iPlace<<"; "<<"transferCut = "<<transferCut<<G4endl;
  // G4cout<<"size = "<<G4int((*fPAIdEdxTable)(iPlace)->GetVectorLength())
  //           <<G4endl;  
  for( iTransfer = 0 ; 
       iTransfer < G4int((*fPAIdEdxTable)(iPlace)->GetVectorLength()) ; 
       iTransfer++)
  {
    if(transferCut <= (*fPAIdEdxTable)(iPlace)->GetLowEdgeEnergy(iTransfer))
    {
      break ;
    }
  }  
  if ( iTransfer >= G4int((*fPAIdEdxTable)(iPlace)->GetVectorLength()) )
  {
      iTransfer = (*fPAIdEdxTable)(iPlace)->GetVectorLength() - 1 ;
  }
  y1 = (*(*fPAIdEdxTable)(iPlace))(iTransfer-1) ;
  y2 = (*(*fPAIdEdxTable)(iPlace))(iTransfer) ;
  // G4cout<<"y1 = "<<y1<<"; "<<"y2 = "<<y2<<G4endl;
  x1 = (*fPAIdEdxTable)(iPlace)->GetLowEdgeEnergy(iTransfer-1) ;
  x2 = (*fPAIdEdxTable)(iPlace)->GetLowEdgeEnergy(iTransfer) ;
  // G4cout<<"x1 = "<<x1<<"; "<<"x2 = "<<x2<<G4endl;

  if ( y1 == y2 )    dEdxCut = y2 ;
  else
  {
    //  if ( x1 == x2  ) dEdxCut = y1 + (y2 - y1)*G4UniformRand() ;
    if ( abs(x1-x2) <= eV  ) dEdxCut = y1 + (y2 - y1)*G4UniformRand() ;
    else             dEdxCut = y1 + (transferCut - x1)*(y2 - y1)/(x2 - x1) ;      
  }
  //  G4cout<<""<<dEdxCut<<G4endl;
  return dEdxCut ;
}

//////////////////////////////////////////////////////////////////////////////

G4double G4PAIPhotonModel::ComputeDEDX(const G4MaterialCutsCouple* matCC,
                                 const G4ParticleDefinition* p,
                                       G4double kineticEnergy,
                                       G4double cutEnergy)
{
  G4int iTkin,iPlace;
  size_t jMat;
  G4double scaledTkin = kineticEnergy*p->GetPDGMass()/proton_mass_c2;
  G4double charge     = p->GetPDGCharge();
  G4double charge2    = charge*charge, dEdx;

  for( jMat = 0 ;jMat < fMaterialCutsCoupleVector.size() ; ++jMat )
  {
    if( matCC == fMaterialCutsCoupleVector[jMat] ) break;
  }
  if(jMat == fMaterialCutsCoupleVector.size() && jMat > 0) jMat--;

  fPAIdEdxTable = fPAIdEdxBank[jMat];
  fdEdxVector = fdEdxTable[jMat];
  for(iTkin = 0 ; iTkin < fTotBin ; iTkin++)
  {
    if(scaledTkin < fProtonEnergyVector->GetLowEdgeEnergy(iTkin)) break ;    
  }
  iPlace = iTkin - 1;
  if(iPlace < 0) iPlace = 0;
  dEdx = charge2*( (*fdEdxVector)(iPlace) - GetdEdxCut(iPlace,cutEnergy) ) ;  

  if( dEdx < 0.) dEdx = 0.;
  return dEdx;
}

/////////////////////////////////////////////////////////////////////////

G4double G4PAIPhotonModel::CrossSection( const G4MaterialCutsCouple* matCC,
                                   const G4ParticleDefinition* p,
                                         G4double kineticEnergy,
                                         G4double cutEnergy,
                                         G4double maxEnergy  ) 
{
  G4int iTkin,iPlace;
  size_t jMat, jMatCC;
  G4double tmax = std::min(MaxSecondaryEnergy(p, kineticEnergy), maxEnergy);
  G4double scaledTkin = kineticEnergy*p->GetPDGMass()/proton_mass_c2;
  G4double charge     = p->GetPDGCharge();
  G4double charge2    = charge*charge, cross, cross1, cross2;
  G4double photon1, photon2, plasmon1, plasmon2;

  const G4ProductionCutsTable* theCoupleTable=
        G4ProductionCutsTable::GetProductionCutsTable();

  size_t numOfCouples = theCoupleTable->GetTableSize();

  for (jMatCC = 0 ; jMatCC < numOfCouples ; jMatCC++ )
  {
    if( matCC == theCoupleTable->GetMaterialCutsCouple(jMatCC) ) break;
  }
  if( jMatCC == numOfCouples && jMatCC > 0 ) jMatCC--;

  const std::vector<G4double>*  photonCutInKineticEnergy = theCoupleTable->
                                GetEnergyCutsVector(idxG4GammaCut);

  G4double photonCut = (*photonCutInKineticEnergy)[jMatCC] ;

  for( jMat = 0 ;jMat < fMaterialCutsCoupleVector.size() ; ++jMat )
  {
    if( matCC == fMaterialCutsCoupleVector[jMat] ) break;
  }
  if(jMat == fMaterialCutsCoupleVector.size() && jMat > 0) jMat--;

  fPAItransferTable = fPAIxscBank[jMat];
  fPAIphotonTable   = fPAIphotonBank[jMat];
  fPAIplasmonTable  = fPAIplasmonBank[jMat];

  for(iTkin = 0 ; iTkin < fTotBin ; iTkin++)
  {
    if(scaledTkin < fProtonEnergyVector->GetLowEdgeEnergy(iTkin)) break ;    
  }
  iPlace = iTkin - 1;
  if(iPlace < 0) iPlace = 0;

  photon1 = GetdNdxPhotonCut(iPlace,tmax);  
  photon2 = GetdNdxPhotonCut(iPlace,photonCut); 
 
  plasmon1 = GetdNdxPlasmonCut(iPlace,tmax);  
  plasmon2 = GetdNdxPlasmonCut(iPlace,cutEnergy); 
 
  cross1 = photon1 + plasmon1;    
  cross2 = photon2 + plasmon2;    
  cross  = (cross2 - cross1)*charge2;

  if( cross < 0. ) cross = 0.;
  return cross;
}

///////////////////////////////////////////////////////////////////////////
//
// It is analog of PostStepDoIt in terms of secondary electron or photon to
// be returned as G4Dynamicparticle*.
//

G4DynamicParticle* 
G4PAIPhotonModel::SampleSecondary( const G4MaterialCutsCouple* matCC,
                                   const G4DynamicParticle* dp,
                                         G4double tmin,
                                         G4double maxEnergy)
{
  size_t jMat;
  for( jMat = 0 ;jMat < fMaterialCutsCoupleVector.size() ; ++jMat )
  {
    if( matCC == fMaterialCutsCoupleVector[jMat] ) break;
  }
  if( jMat == fMaterialCutsCoupleVector.size() && jMat > 0 ) jMat--;

  fPAItransferTable = fPAIxscBank[jMat];
  fPAIphotonTable   = fPAIphotonBank[jMat];
  fPAIplasmonTable  = fPAIplasmonBank[jMat];

  fdNdxCutVector        = fdNdxCutTable[jMat];
  fdNdxCutPhotonVector  = fdNdxCutPhotonTable[jMat];
  fdNdxCutPlasmonVector = fdNdxCutPlasmonTable[jMat];

  G4double tmax = std::min(MaxSecondaryEnergy(dp), maxEnergy);
  if( tmin >= tmax ) return 0;

  G4ThreeVector momentum = dp->GetMomentumDirection();
  G4double particleMass  = dp->GetMass();
  G4double kineticEnergy = dp->GetKineticEnergy();
  G4double scaledTkin    = kineticEnergy*particleMass/proton_mass_c2;
  G4double totalEnergy   = kineticEnergy + particleMass;
  G4double pSquare       = kineticEnergy*(totalEnergy+particleMass);

  G4int iTkin;
  for(iTkin=0;iTkin<fTotBin;iTkin++)
  {
    if(scaledTkin < fProtonEnergyVector->GetLowEdgeEnergy(iTkin))  break ;
  }
  G4int iPlace = iTkin - 1 ;
  if(iPlace < 0) iPlace = 0;

  G4double dNdxPhotonCut  = (*fdNdxCutPhotonVector)(iPlace) ;  
  G4double dNdxPlasmonCut = (*fdNdxCutPlasmonVector)(iPlace) ;  
  G4double dNdxCut        = dNdxPhotonCut  + dNdxPlasmonCut;
 
  G4double ratio;
  if (dNdxCut > 0.) ratio = dNdxPhotonCut/dNdxCut;
  else              ratio = 0.;

  if(ratio < G4UniformRand() ) // secondary e-
  {
    G4double deltaTkin     = GetPostStepTransfer(fPAIplasmonTable, fdNdxCutPlasmonVector,
                                                 iPlace, scaledTkin);

    //G4cout<<"PAIPhotonModel PlasmonPostStepTransfer = "<<deltaTkin/keV<<" keV"<<G4endl ; 

    if( deltaTkin <= 0. ) return 0;

    G4double deltaTotalMomentum = sqrt(deltaTkin*(deltaTkin + 2. * electron_mass_c2 ));
    G4double totalMomentum      = sqrt(pSquare);
    G4double costheta           = deltaTkin*(totalEnergy + electron_mass_c2)
                                /(deltaTotalMomentum * totalMomentum);
    if (costheta < 0.) costheta = 0.;
    if (costheta > +1.) costheta = +1.;

    //  direction of the delta electron
  
    G4double phi = twopi*G4UniformRand(); 
    G4double sintheta = sqrt((1.+costheta)*(1.-costheta));
    G4double dirx = sintheta*cos(phi), diry = sintheta*sin(phi), dirz = costheta;

    G4ThreeVector deltaDirection(dirx,diry,dirz);
    deltaDirection.rotateUz(momentum);

    // create G4DynamicParticle object for e- delta ray
 
    G4DynamicParticle* deltaRay = new G4DynamicParticle;
    deltaRay->SetDefinition(G4Electron::Electron());
    deltaRay->SetKineticEnergy( deltaTkin );
    deltaRay->SetMomentumDirection(deltaDirection); 

    return deltaRay;
  }
  else    // secondary 'Cherenkov' photon
  { 
    G4double deltaTkin     = GetPostStepTransfer(fPAIphotonTable, fdNdxCutPhotonVector,
                                                 iPlace,scaledTkin);

    //G4cout<<"PAIPhotonModel PhotonPostStepTransfer = "<<deltaTkin/keV<<" keV"<<G4endl ; 

    if( deltaTkin <= 0. ) return 0;

    // G4double deltaTotalMomentum = sqrt(deltaTkin*(deltaTkin + 2. * electron_mass_c2 ));
    // G4double totalMomentum      = sqrt(pSquare);
    //  deltaTkin*(totalEnergy + electron_mass_c2)
    //                        /(deltaTotalMomentum * totalMomentum);

    G4double costheta            = G4UniformRand();
    if (costheta < 0.)  costheta = 0.;
    if (costheta > 1.) costheta = 1.;

    //  direction of the 'Cherenkov' photon
  
    G4double phi = twopi*G4UniformRand(); 
    G4double sintheta = sqrt((1.+costheta)*(1.-costheta));
    G4double dirx = sintheta*cos(phi), diry = sintheta*sin(phi), dirz = costheta;

    G4ThreeVector deltaDirection(dirx,diry,dirz);
    deltaDirection.rotateUz(momentum);

    // create G4DynamicParticle object for photon ray
 
    G4DynamicParticle* photonRay = new G4DynamicParticle;
    photonRay->SetDefinition(G4Gamma::Gamma());
    photonRay->SetKineticEnergy( deltaTkin );
    photonRay->SetMomentumDirection(deltaDirection); 

    return photonRay;    
  }
}


///////////////////////////////////////////////////////////////////////
//
// Returns post step PAI energy transfer > cut electron/photon energy according to passed 
// scaled kinetic energy of particle

G4double  
G4PAIPhotonModel::GetPostStepTransfer( G4PhysicsTable* pTable,
				       G4PhysicsLogVector* pVector,
                                       G4int iPlace, G4double scaledTkin )
{  
  // G4cout<<"G4PAIPhotonModel::GetPostStepTransfer"<<G4endl ;

  G4int iTkin = iPlace+1, iTransfer;
  G4double transfer = 0.0, position, dNdxCut1, dNdxCut2, E1, E2, W1, W2, W ;

  dNdxCut1 = (*pVector)(iPlace) ;  

  //  G4cout<<"iPlace = "<<iPlace<<endl ;

  if(iTkin == fTotBin) // Fermi plato, try from left
  {
      position = dNdxCut1*G4UniformRand() ;

      for( iTransfer = 0;
 iTransfer < G4int((*pTable)(iPlace)->GetVectorLength()); iTransfer++ )
      {
        if(position >= (*(*pTable)(iPlace))(iTransfer)) break ;
      }
      transfer = GetEnergyTransfer(pTable,iPlace,position,iTransfer);
  }
  else
  {
    dNdxCut2 = (*pVector)(iPlace+1) ;  
    if(iTkin == 0) // Tkin is too small, trying from right only
    {
      position = dNdxCut2*G4UniformRand() ;

      for( iTransfer = 0;
  iTransfer < G4int((*pTable)(iPlace+1)->GetVectorLength()); iTransfer++ )
      {
        if(position >= (*(*pTable)(iPlace+1))(iTransfer)) break ;
      }
      transfer = GetEnergyTransfer(pTable,iPlace+1,position,iTransfer);
    } 
    else // general case: Tkin between two vectors of the material
    {
      E1 = fProtonEnergyVector->GetLowEdgeEnergy(iTkin - 1) ; 
      E2 = fProtonEnergyVector->GetLowEdgeEnergy(iTkin)     ;
      W  = 1.0/(E2 - E1) ;
      W1 = (E2 - scaledTkin)*W ;
      W2 = (scaledTkin - E1)*W ;

      position = ( dNdxCut1*W1 + dNdxCut2*W2 )*G4UniformRand() ;

        // G4cout<<position<<"\t" ;

      for( iTransfer = 0;
 iTransfer < G4int((*pTable)(iPlace)->GetVectorLength()); iTransfer++ )
      {
          if( position >=
          ( (*(*pTable)(iPlace))(iTransfer)*W1 +
            (*(*pTable)(iPlace+1))(iTransfer)*W2) ) break ;
      }
      transfer = GetEnergyTransfer(pTable,iPlace,position,iTransfer);
    }
  } 
  //  G4cout<<"PAIPhotonModel PostStepTransfer = "<<transfer/keV<<" keV"<<G4endl ; 
  if(transfer < 0.0 ) transfer = 0.0 ;
  return transfer ;
}

///////////////////////////////////////////////////////////////////////
//
// Returns random PAI energy transfer according to passed 
// indexes of particle 

G4double
G4PAIPhotonModel::GetEnergyTransfer( G4PhysicsTable* pTable, G4int iPlace, 
                                     G4double position, G4int iTransfer )
{ 
  G4double x1, x2, y1, y2, energyTransfer ;

  if(iTransfer == 0)
  {
    energyTransfer = (*pTable)(iPlace)->GetLowEdgeEnergy(iTransfer) ;
  }  
  else
  {
    if ( iTransfer >= G4int((*pTable)(iPlace)->GetVectorLength()) )
    {
      iTransfer = (*pTable)(iPlace)->GetVectorLength() - 1 ;
    }
    y1 = (*(*pTable)(iPlace))(iTransfer-1) ;
    y2 = (*(*fPAItransferTable)(iPlace))(iTransfer) ;

    x1 = (*pTable)(iPlace)->GetLowEdgeEnergy(iTransfer-1) ;
    x2 = (*pTable)(iPlace)->GetLowEdgeEnergy(iTransfer) ;

    if ( x1 == x2 )    energyTransfer = x2 ;
    else
    {
      if ( y1 == y2  ) energyTransfer = x1 + (x2 - x1)*G4UniformRand() ;
      else
      {
        energyTransfer = x1 + (position - y1)*(x2 - x1)/(y2 - y1) ;
      }
    }
  }
  return energyTransfer ;
}


////////////////////////////////////////////////////////////////////////////

std::vector<G4DynamicParticle*>* 
G4PAIPhotonModel::SampleSecondaries( const G4MaterialCutsCouple* couple,
                               const G4DynamicParticle* dp,
                                     G4double tmin,
                                     G4double maxEnergy)
{
  std::vector<G4DynamicParticle*>* vdp = new std::vector<G4DynamicParticle*>;
  G4DynamicParticle* delta             = SampleSecondary(couple, dp, tmin, maxEnergy);
  vdp->push_back(delta);
  return vdp;
}

///////////////////////////////////////////////////////////////////////
//
// Works like AlongStepDoIt method of process family




G4double G4PAIPhotonModel::SampleFluctuations( const G4Material* material,
                                         const G4DynamicParticle* aParticle,
				               G4double&,
					       G4double& step,
                                               G4double&)
{
  size_t jMat;
  for( jMat = 0 ;jMat < fMaterialCutsCoupleVector.size() ; ++jMat )
  {
    if( material == fMaterialCutsCoupleVector[jMat]->GetMaterial() ) break;
  }
  if(jMat == fMaterialCutsCoupleVector.size() && jMat > 0) jMat--;

  fPAItransferTable = fPAIxscBank[jMat];
  fPAIphotonTable = fPAIphotonBank[jMat];
  fPAIplasmonTable = fPAIplasmonBank[jMat];

  fdNdxCutVector   = fdNdxCutTable[jMat];
  fdNdxCutPhotonVector   = fdNdxCutPhotonTable[jMat];
  fdNdxCutPlasmonVector   = fdNdxCutPlasmonTable[jMat];

  G4int iTkin, iPlace  ;

  // G4cout<<"G4PAIPhotonModel::SampleFluctuations"<<G4endl ;

  G4double loss, photonLoss, plasmonLoss, charge2 ;
 

  G4double Tkin       = aParticle->GetKineticEnergy() ;
  G4double MassRatio  = proton_mass_c2/aParticle->GetDefinition()->GetPDGMass() ;
  G4double charge     = aParticle->GetDefinition()->GetPDGCharge() ;
  charge2             = charge*charge ;
  G4double scaledTkin = Tkin*MassRatio ;
  G4double cof        = step*charge2;

  for( iTkin = 0; iTkin < fTotBin; iTkin++)
  {
    if(scaledTkin < fProtonEnergyVector->GetLowEdgeEnergy(iTkin))   break ;
  }
  iPlace = iTkin - 1 ; 
  if( iPlace < 0 ) iPlace = 0;

  photonLoss = GetAlongStepTransfer(fPAIphotonTable,fdNdxCutPhotonVector,iPlace,scaledTkin,cof);

  //  G4cout<<"PAIPhotonModel AlongStepPhotonLoss = "<<photonLoss/keV<<" keV"<<G4endl ; 

  plasmonLoss = GetAlongStepTransfer(fPAIplasmonTable,fdNdxCutPlasmonVector,iPlace,scaledTkin,cof);

  //  G4cout<<"PAIPhotonModel AlongStepPlasmonLoss = "<<plasmonLoss/keV<<" keV"<<G4endl ; 

  loss = photonLoss + plasmonLoss;

  //  G4cout<<"PAIPhotonModel AlongStepLoss = "<<loss/keV<<" keV"<<G4endl ; 

  return loss;
}

///////////////////////////////////////////////////////////////////////
//
// Returns along step PAI energy transfer < cut electron/photon energy according to passed 
// scaled kinetic energy of particle and cof = step*charge*charge

G4double  
G4PAIPhotonModel::GetAlongStepTransfer( G4PhysicsTable* pTable,
				        G4PhysicsLogVector* pVector,
                                        G4int iPlace, G4double scaledTkin,
                                        G4double cof )
{  
  G4int iTkin = iPlace + 1, iTransfer;
  G4double loss = 0., position, E1, E2, W1, W2, W, dNdxCut1, dNdxCut2, meanNumber;
  G4long numOfCollisions;

  dNdxCut1 = (*pVector)(iPlace) ;  

  //  G4cout<<"iPlace = "<<iPlace<<endl ;

  if(iTkin == fTotBin) // Fermi plato, try from left
  {
    meanNumber = ((*(*pTable)(iPlace))(0) - dNdxCut1)*cof;
    if(meanNumber < 0.) meanNumber = 0. ;
    numOfCollisions = RandPoisson::shoot(meanNumber) ;
    
    //     G4cout<<"numOfCollisions = "<<numOfCollisions<<G4endl ;

    while(numOfCollisions)
    {
      position = dNdxCut1+
                 ((*(*pTable)(iPlace))(0) - dNdxCut1)*G4UniformRand() ;

      for( iTransfer = 0;
   iTransfer < G4int((*pTable)(iPlace)->GetVectorLength()); iTransfer++ )
      {
        if(position >= (*(*pTable)(iPlace))(iTransfer)) break ;
      }
      loss += GetEnergyTransfer(pTable,iPlace,position,iTransfer);
      numOfCollisions-- ;
    }
  }
  else
  {
    dNdxCut2 = (*pVector)(iPlace+1) ; 
 
    if(iTkin == 0) // Tkin is too small, trying from right only
    {
      meanNumber = ((*(*pTable)(iPlace+1))(0) - dNdxCut2)*cof;
      if( meanNumber < 0. ) meanNumber = 0. ;
      numOfCollisions = RandPoisson::shoot(meanNumber) ;

      //  G4cout<<"numOfCollisions = "<<numOfCollisions<<G4endl ;

      while(numOfCollisions)
      {
        position = dNdxCut2+
                   ((*(*pTable)(iPlace+1))(0) - dNdxCut2)*G4UniformRand();
   
        for( iTransfer = 0;
   iTransfer < G4int((*pTable)(iPlace+1)->GetVectorLength()); iTransfer++ )
        {
          if(position >= (*(*pTable)(iPlace+1))(iTransfer)) break ;
        }
        loss += GetEnergyTransfer(pTable,iPlace+1,position,iTransfer);
        numOfCollisions-- ;
      }
    } 
    else // general case: Tkin between two vectors of the material
    {
      E1 = fProtonEnergyVector->GetLowEdgeEnergy(iTkin - 1) ; 
      E2 = fProtonEnergyVector->GetLowEdgeEnergy(iTkin)     ;
       W = 1.0/(E2 - E1) ;
      W1 = (E2 - scaledTkin)*W ;
      W2 = (scaledTkin - E1)*W ;

      // G4cout<<"(*(*pTable)(iPlace))(0) = "<<
      //   (*(*pTable)(iPlace))(0)<<G4endl ;
      // G4cout<<"(*(*pTable)(iPlace+1))(0) = "<<
      //     (*(*pTable)(iPlace+1))(0)<<G4endl ;

      meanNumber=( ((*(*pTable)(iPlace))(0)-dNdxCut1)*W1 + 
		   ((*(*pTable)(iPlace+1))(0)-dNdxCut2)*W2 )*cof;
      if(meanNumber<0.0) meanNumber = 0.0;
      numOfCollisions = RandPoisson::shoot(meanNumber) ;

      //  G4cout<<"numOfCollisions = "<<numOfCollisions<<endl ;

      while(numOfCollisions)
      {
        position =( (dNdxCut1+
                  ((*(*pTable)(iPlace  ))(0)-dNdxCut1))*W1 + 
                    (dNdxCut2+
                  ((*(*pTable)(iPlace+1))(0)-dNdxCut2))*W2 )*G4UniformRand();

        // G4cout<<position<<"\t" ;

        for( iTransfer = 0;
    iTransfer < G4int((*pTable)(iPlace)->GetVectorLength()); iTransfer++ )
        {
          if( position >=
          ( (*(*pTable)(iPlace))(iTransfer)*W1 + 
            (*(*pTable)(iPlace+1))(iTransfer)*W2) )
          {
	      break ;
	  }
        }
	// loss += (*pTable)(iPlace)->GetLowEdgeEnergy(iTransfer) ; 
        loss += GetEnergyTransfer(pTable,iPlace,position,iTransfer);
        numOfCollisions-- ;    
      }
    }
  } 

  return loss ;

}

//////////////////////////////////////////////////////////////////////
//
// Returns the statistical estimation of the energy loss distribution variance
//


G4double G4PAIPhotonModel::Dispersion( const G4Material* material, 
                                 const G4DynamicParticle* aParticle,
 				       G4double& tmax, 
			               G4double& step       )
{
  G4double loss, sumLoss=0., sumLoss2=0., sigma2, meanLoss=0.;
  for(G4int i = 0 ; i < fMeanNumber; i++)
  {
    loss      = SampleFluctuations(material,aParticle,tmax,step,meanLoss);
    sumLoss  += loss;
    sumLoss2 += loss*loss;
  }
  meanLoss = sumLoss/fMeanNumber;
  sigma2   = meanLoss*meanLoss + (sumLoss2-2*sumLoss*meanLoss)/fMeanNumber;
  return sigma2;
}


//
//
/////////////////////////////////////////////////





