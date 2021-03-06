/*=========================================================================

 Program: FEMUS
 Module: Mesh
 Authors: Eugenio Aulisa

 Copyright (c) FEMTTU
 All rights reserved.

 This software is distributed WITHOUT ANY WARRANTY; without even
 the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 PURPOSE.  See the above copyright notice for more information.

=========================================================================*/

//----------------------------------------------------------------------------
// includes :
//----------------------------------------------------------------------------
#include "Mesh.hpp"
#include "MeshGeneration.hpp"
#include "MeshMetisPartitioning.hpp"
#include "GambitIO.hpp"
#include "SalomeIO.hpp"
#include "NumericVector.hpp"

// C++ includes
#include <iostream>
#include <fstream>
#include <cstdlib>
#include <cmath>
#include <cstring>
#include <algorithm>


namespace femus {

using std::cout;
using std::endl;
using std::min;
using std::sort;
using std::map;

bool Mesh::_IsUserRefinementFunctionDefined = false;

unsigned Mesh::_dimension=2;
unsigned Mesh::_ref_index=4;  // 8*DIM[2]+4*DIM[1]+2*DIM[0];
unsigned Mesh::_face_index=2; // 4*DIM[2]+2*DIM[1]+1*DIM[0];

//------------------------------------------------------------------------------------------------------
  Mesh::Mesh(){

    _coarseMsh = NULL;

    for(int i=0;i<5;i++){
      _ProjCoarseToFine[i]=NULL;
    }

    for (int itype=0; itype<3; itype++) {
      for (int jtype=0; jtype<3; jtype++) {
	_ProjQitoQj[itype][jtype] = NULL;
      }
    }
  }


  Mesh::~Mesh(){
    delete el;
    _topology->FreeSolutionVectors();
    delete _topology;

    for (int itype=0; itype<3; itype++) {
      for (int jtype=0; jtype<3; jtype++) {
	if(_ProjQitoQj[itype][jtype]){
	  delete _ProjQitoQj[itype][jtype];
	  _ProjQitoQj[itype][jtype] = NULL;
	}
      }
    }

    for (unsigned i=0; i<5; i++) {
      if (_ProjCoarseToFine[i]) {
	delete _ProjCoarseToFine[i];
	_ProjCoarseToFine[i]=NULL;
      }
    }
  }

/// print Mesh info
void Mesh::PrintInfo() {

 std::cout << " Mesh Level        : " << _level  << std::endl;
 std::cout << " Number of elements: " << _nelem  << std::endl;
 std::cout << " Number of nodes   : " << _nnodes << std::endl;

}

/**
 *  This function generates the coarse Mesh level, $l_0$, from an input Mesh file (Now only the Gambit Neutral File)
 **/
void Mesh::ReadCoarseMesh(const std::string& name, const double Lref, std::vector<bool> &type_elem_flag) {

  _coords.resize(3);

  _level = 0;

  if(name.rfind(".neu") < name.size())
  {
    GambitIO(*this).read(name,_coords,Lref,type_elem_flag);
  }
  else if(name.rfind(".med") < name.size()) {
    SalomeIO(*this).read(name,_coords,Lref,type_elem_flag);
  }
  else
  {
    std::cerr << " ERROR: Unrecognized file extension: " << name
	      << "\n   I understand the following:\n\n"
	      << "     *.neu -- Gambit Neutral File\n"
              << std::endl;
  }

  el->SetNodeNumber(_nnodes);

  std::vector < int > partition;
  partition.reserve(GetNumberOfNodes());
  partition.resize(GetNumberOfElements());
  MeshMetisPartitioning meshMetisPartitioning(*this);
  meshMetisPartitioning.DoPartition(partition, false);
  FillISvector(partition);
  partition.resize(0);


  BuildAdjVtx();
  Buildkel();

  _topology = new Solution(this);

  _topology->AddSolution("X",LAGRANGE,SECOND,1,0);
  _topology->AddSolution("Y",LAGRANGE,SECOND,1,0);
  _topology->AddSolution("Z",LAGRANGE,SECOND,1,0);

  _topology->ResizeSolutionVector("X");
  _topology->ResizeSolutionVector("Y");
  _topology->ResizeSolutionVector("Z");

  _topology->GetSolutionName("X") = _coords[0];
  _topology->GetSolutionName("Y") = _coords[1];
  _topology->GetSolutionName("Z") = _coords[2];

  _topology->AddSolution("AMR",DISCONTINOUS_POLYNOMIAL,ZERO,1,0);

  _topology->ResizeSolutionVector("AMR");

  _topology->AddSolution("Material", DISCONTINOUS_POLYNOMIAL, ZERO, 1 , 0);
  _topology->AddSolution("Group", DISCONTINOUS_POLYNOMIAL, ZERO, 1 , 0);
  _topology->AddSolution("Type", DISCONTINOUS_POLYNOMIAL, ZERO, 1 , 0);

  _topology->ResizeSolutionVector("Material");
  _topology->ResizeSolutionVector("Group");
  _topology->ResizeSolutionVector("Type");

  NumericVector &material =  _topology->GetSolutionName("Material");
  NumericVector &group =  _topology->GetSolutionName("Group");
  NumericVector &type =  _topology->GetSolutionName("Type");


  for (int iel = _elementOffset[_iproc]; iel < _elementOffset[_iproc + 1]; iel++) {
    group.set( iel, el->GetElementGroup(iel) );
    type.set( iel, el->GetElementType(iel) );
    material.set(iel,el->GetElementMaterial(iel));
  }

  material.close();
  group.close();
  type.close();

  el->deleteParallelizedQuantities();

};

/**
 *  This function generates the coarse Box Mesh level using the built-in generator
 **/
void Mesh::GenerateCoarseBoxMesh(
        const unsigned int nx, const unsigned int ny, const unsigned int nz,
        const double xmin, const double xmax,
        const double ymin, const double ymax,
        const double zmin, const double zmax,
        const ElemType elemType, std::vector<bool> &type_elem_flag) {

  _coords.resize(3);

  _level = 0;

  MeshTools::Generation::BuildBox(*this,_coords,nx,ny,nz,xmin,xmax,ymin,ymax,zmin,zmax,elemType,type_elem_flag);

  el->SetNodeNumber(_nnodes);


  std::vector < int > partition;
  partition.reserve(GetNumberOfNodes());
  partition.resize(GetNumberOfElements());
  MeshMetisPartitioning meshMetisPartitioning(*this);
  meshMetisPartitioning.DoPartition(partition, false);
  FillISvector(partition);
  partition.resize(0);

  BuildAdjVtx();

  Buildkel();

  _topology = new Solution(this);

  _topology->AddSolution("X",LAGRANGE,SECOND,1,0);
  _topology->AddSolution("Y",LAGRANGE,SECOND,1,0);
  _topology->AddSolution("Z",LAGRANGE,SECOND,1,0);

  _topology->ResizeSolutionVector("X");
  _topology->ResizeSolutionVector("Y");
  _topology->ResizeSolutionVector("Z");

  _topology->GetSolutionName("X") = _coords[0];
  _topology->GetSolutionName("Y") = _coords[1];
  _topology->GetSolutionName("Z") = _coords[2];

  _topology->AddSolution("AMR",DISCONTINOUS_POLYNOMIAL,ZERO,1,0);

  _topology->ResizeSolutionVector("AMR");


  _topology->AddSolution("Material", DISCONTINOUS_POLYNOMIAL, ZERO, 1 , 0);
  _topology->AddSolution("Group", DISCONTINOUS_POLYNOMIAL, ZERO, 1 , 0);
  _topology->AddSolution("Type", DISCONTINOUS_POLYNOMIAL, ZERO, 1 , 0);

  _topology->ResizeSolutionVector("Material");
  _topology->ResizeSolutionVector("Group");
  _topology->ResizeSolutionVector("Type");

  NumericVector &material =  _topology->GetSolutionName("Material");
  NumericVector &group =  _topology->GetSolutionName("Group");
  NumericVector &type =  _topology->GetSolutionName("Type");

  for (int iel = _elementOffset[_iproc]; iel < _elementOffset[_iproc + 1]; iel++) {
    group.set( iel, el->GetElementGroup(iel) );
    type.set( iel, el->GetElementType(iel) );
    material.set(iel,el->GetElementMaterial(iel));
  }

  material.close();
  group.close();
  type.close();

  el->deleteParallelizedQuantities();
}


/**
 * This function searches all the elements around all the vertices
 **/
void Mesh::BuildAdjVtx() {
  el->AllocateVertexElementMemory();
  for (unsigned iel=0; iel<_nelem; iel++) {
    for (unsigned inode=0; inode < el->GetElementDofNumber(iel,0); inode++) {
      unsigned irow=el->GetElementVertexIndex(iel,inode)-1u;
      unsigned jcol=0;
      while ( 0 != el->GetVertexElementIndex(irow,jcol) ) jcol++;
      el->SetVertexElementIndex(irow,jcol,iel+1u);
    }
  }
}

/**
 * This function stores the element adiacent to the element face (iel,iface)
 * and stores it in kel[iel][iface]
 **/
void Mesh::Buildkel() {
  for (unsigned iel=0; iel<el->GetElementNumber(); iel++) {
    for (unsigned iface=0; iface<el->GetElementFaceNumber(iel); iface++) {
      if ( el->GetFaceElementIndex(iel,iface) <= 0) {//TODO probably just == -1
        unsigned i1=el->GetFaceVertexIndex(iel,iface,0);
        unsigned i2=el->GetFaceVertexIndex(iel,iface,1);
        unsigned i3=el->GetFaceVertexIndex(iel,iface,2);
        for (unsigned j=0; j< el->GetVertexElementNumber(i1-1u); j++) {
          unsigned jel= el->GetVertexElementIndex(i1-1u,j)-1u;
          if (jel > iel) {
            for (unsigned jface=0; jface<el->GetElementFaceNumber(jel); jface++) {
              if ( el->GetFaceElementIndex(jel,jface) <= 0) {
                unsigned j1=el->GetFaceVertexIndex(jel,jface,0);
                unsigned j2=el->GetFaceVertexIndex(jel,jface,1);
                unsigned j3=el->GetFaceVertexIndex(jel,jface,2);
                unsigned j4=el->GetFaceVertexIndex(jel,jface,3);
                if ((Mesh::_dimension==3 &&
                     (i1==j1 || i1==j2 || i1==j3 ||  i1==j4 )&&
                     (i2==j1 || i2==j2 || i2==j3 ||  i2==j4 )&&
                     (i3==j1 || i3==j2 || i3==j3 ||  i3==j4 ))||
                    (Mesh::_dimension==2 &&
                     (i1==j1 || i1==j2 )&&
                     (i2==j1 || i2==j2 ))||
                    (Mesh::_dimension==1 &&
                     (i1==j1))
                   ) {
                  el->SetFaceElementIndex(iel,iface,jel+1u);
                  el->SetFaceElementIndex(jel,jface,iel+1u);
                }
              }
            }
          }
        }
      }
    }
  }
}


void Mesh::AllocateAndMarkStructureNode() {
  el->AllocateNodeRegion();

  vector <double> localizedElementMaterial;
  _topology->_Sol[_materialIndex]->localize_to_all(localizedElementMaterial);
  
   vector <double> localizedElementType;
  _topology->_Sol[_typeIndex]->localize_to_all(localizedElementType);

  for (unsigned iel=0; iel<_nelem; iel++) {

    //int flag_mat = el->GetElementMaterial(iel);
    int flag_mat = static_cast < short unsigned > (localizedElementMaterial[iel]+ 0.25);

    if (flag_mat==4) {
      unsigned nve = el->GetNVE(localizedElementType[iel],2);
      for ( unsigned i=0; i<nve; i++) {
        unsigned inode=el->GetElementVertexIndex(iel,i)-1u;
        el->SetNodeRegion(inode, 1);
      }
    }
  }
}


void Mesh::SetFiniteElementPtr(const elem_type * OtherFiniteElement[6][5]){
  for(int i=0;i<6;i++)
    for(int j=0;j<5;j++)
      _finiteElement[i][j] = OtherFiniteElement[i][j];
}


  // *******************************************************

//dof map: piecewise liner 0, quadratic 1, bi-quadratic 2, piecewise constant 3, piecewise linear discontinuous 4

void Mesh::FillISvector(vector < int > &partition) {

  //BEGIN Initialization for k = 0,1,2,3,4

  std::vector < unsigned > mapping;
  mapping.reserve(GetNumberOfNodes());

  _elementOffset.resize(_nprocs+1);
  _elementOffset[0] = 0;

  for(int k = 0; k < 5; k++) {
    _dofOffset[k].resize( _nprocs + 1 );
    _dofOffset[k][0] = 0;
  }
  //END Initialization for k = 0,1,2,3,4

  mapping.resize(GetNumberOfElements());

  //BEGIN building the  metis2Gambit_elem and  k = 3,4
  unsigned counter = 0;
  for(int isdom = 0; isdom < _nprocs; isdom++) { // isdom = iprocess
    for(unsigned iel = 0; iel < GetNumberOfElements(); iel++){
      if( partition[iel] == isdom ){
	//filling the Metis to Mesh element mapping
	mapping[ counter ] = iel;
        counter++;
	_elementOffset[isdom + 1] = counter;
      }
    }
  }


  if( GetLevel() == 0 ){
    el->ReorderMeshElements(mapping, NULL);
  }
  else{
    el->ReorderMeshElements(mapping, _coarseMsh->el);
  }

  for(int isdom = 0; isdom < _nprocs; isdom++){
    unsigned localSize = _elementOffset[isdom+1] - _elementOffset[isdom];
    unsigned offsetPWLD = _elementOffset[isdom] * (_dimension + 1);
    for(unsigned iel = _elementOffset[isdom]; iel < _elementOffset[isdom+1]; iel++){
      //piecewise linear discontinuous
      unsigned locIel = iel - _elementOffset[isdom];
      for(unsigned k = 0; k < _dimension + 1; k++){
        unsigned locKel = ( k * localSize ) + locIel;
        unsigned kel = offsetPWLD + locKel;
      }
    }
  }

  // ghost vs owned nodes: 3 and 4 have no ghost nodes
  for(unsigned k = 3; k < 5; k++){
    _ownSize[k].assign(_nprocs,0);
  }

  for(int isdom = 0; isdom < _nprocs; isdom++){
    _ownSize[3][isdom] = _elementOffset[isdom+1] - _elementOffset[isdom];
    _ownSize[4][isdom] = (_elementOffset[isdom+1] - _elementOffset[isdom])*(_dimension+1);
  }

  for(int k = 3; k < 5; k++) {
    _ghostDofs[k].resize(_nprocs);
    for(int isdom = 0; isdom < _nprocs; isdom++) {
      _dofOffset[k][isdom+1] = _dofOffset[k][isdom] + _ownSize[k][isdom];
      _ghostDofs[k][isdom].resize( 0 );
    }
  }
  //END building the  metis2Gambit_elem and  k = 3,4

  //BEGIN building for k = 0,1,2

  // Initialization for k = 0,1,2
  partition.assign( GetNumberOfNodes(), _nprocs );
  mapping.resize( GetNumberOfNodes() );

  for( unsigned k = 0; k < 3; k++){
    _ownSize[k].assign(_nprocs,0);
  }
  counter = 0;
  for(int isdom = 0; isdom < _nprocs; isdom++){
    for( unsigned k = 0; k < 3; k++){
      for( unsigned iel = _elementOffset[isdom]; iel < _elementOffset[isdom+1]; iel++){
	unsigned nodeStart = (k == 0) ? 0 : el->GetElementDofNumber(iel,k-1);
	unsigned nodeEnd = el->GetElementDofNumber(iel,k);
	for ( unsigned inode = nodeStart; inode < nodeEnd; inode++) {
	  unsigned ii = el->GetElementVertexIndex(iel,inode) - 1;
	  if(partition[ii] > isdom) {
	    partition[ii] = isdom;
	    mapping[ii] = counter;
	    counter++;
	    for( int j = k; j < 3; j++){
	      _ownSize[j][isdom]++;
	    }
	  }
	}
      }
    }
  }

  partition.resize(0);

  for(int i = 1 ;i <= _nprocs; i++){
    _dofOffset[2][i]= _dofOffset[2][i-1] + _ownSize[2][i-1];
  }

  el->ReorderMeshNodes( mapping );

  if( GetLevel() == 0 ){
    vector <double> coords_temp;
    for(int i = 0;i < 3; i++){
      coords_temp = _coords[i];
        for(unsigned j = 0; j < GetNumberOfNodes(); j++) {
	  _coords[i][mapping[j]] = coords_temp[j];
      }
    }
  }
  mapping.resize(0);
  //END building for k = 2, but incomplete for k = 0, 1

  //BEGIN ghost nodes search k = 0, 1, 2
  for(int k = 0; k < 3; k++){
    _ghostDofs[k].resize(_nprocs);
    for(int isdom = 0; isdom < _nprocs; isdom++){
      std::map < unsigned, bool > ghostMap;
      for(unsigned iel = _elementOffset[isdom]; iel < _elementOffset[isdom+1]; iel++){
	for (unsigned inode = 0; inode < el->GetElementDofNumber(iel,k); inode++) {
	  unsigned ii = el->GetElementVertexIndex(iel,inode)-1;
	  if(ii < _dofOffset[2][isdom]){
	    ghostMap[ii] = true;
	  }
	}
      }
      _ghostDofs[k][isdom].resize( ghostMap.size() );
      unsigned counter = 0;
      for( std::map < unsigned, bool >::iterator it = ghostMap.begin(); it != ghostMap.end(); it++ ){
	_ghostDofs[k][isdom][counter] = it->first;
	counter++;
      }
    }
  }
  //END ghost nodes search k = 0, 1, 2


  //BEGIN completing k = 0, 1

  for(unsigned k = 0; k < 2; k++){

    std::vector < unsigned > ownedGhostCounter( _nprocs , 0);
    unsigned counter = 0;

    _originalOwnSize[k].resize(_nprocs);
    for(int isdom = 0; isdom < _nprocs; isdom++){

      //owned nodes
      for(unsigned inode = _dofOffset[2][isdom]; inode < _ownSize[k][isdom] + _dofOffset[2][isdom]; inode++) {
	counter++;
      }

      for (unsigned inode = 0; inode < _ghostDofs[k][isdom].size(); inode++){
	unsigned ghostNode = _ghostDofs[k][isdom][inode];

	unsigned ksdom = IsdomBisectionSearch(ghostNode, 2);

	int upperBound = _dofOffset[2][ksdom] + _ownSize[k][ksdom];

	if( ghostNode < upperBound ){
	  _ghostDofs[k][isdom][inode] =  ghostNode  - _dofOffset[2][ksdom] + _dofOffset[k][ksdom];
	}
	else if( _ownedGhostMap[k].find(ghostNode) != _ownedGhostMap[k].end() ){
	  _ghostDofs[k][isdom][inode] =  _ownedGhostMap[k][ghostNode];
	}
	else { // owned ghost nodes
	  _ownedGhostMap[k][ ghostNode ] = counter;
	  counter++;
	  ownedGhostCounter[isdom]++;

          for(unsigned jnode = inode; jnode < _ghostDofs[k][isdom].size()-1; jnode++ ){
	    _ghostDofs[k][isdom][jnode] = _ghostDofs[k][isdom][jnode + 1];
	  }

          _ghostDofs[k][isdom].resize(_ghostDofs[k][isdom].size()-1);
	  inode--;
	}
      }
      _originalOwnSize[k][isdom] = _ownSize[k][isdom];
      _ownSize[k][isdom] += ownedGhostCounter[isdom];
      _dofOffset[k][isdom+1] = _dofOffset[k][isdom] + _ownSize[k][isdom];
    }
  }
  //END completing for k = 0, 1

  //delete ghost dof list all but _iproc
  for(int isdom = 0; isdom < _nprocs; isdom++){
    if( isdom != _iproc )
    for(int k = 0; k < 5; k++){
      _ghostDofs[k][isdom].resize(0);
    }
  }


}


  // *******************************************************
  unsigned Mesh::IsdomBisectionSearch(const unsigned &dof, const short unsigned &solType) const{

    unsigned isdom0 = 0;
    unsigned isdom1 = _nprocs ;
    unsigned isdom = _iproc;

    while( dof < _dofOffset[solType][isdom] || dof >= _dofOffset[solType][isdom + 1] ){
      if( dof < _dofOffset[solType][isdom] ) isdom1 = isdom;
      else isdom0 = isdom + 1;
      isdom = ( isdom0 + isdom1 ) / 2;
    }

    return isdom;
  }
  // *******************************************************

  unsigned Mesh::GetSolutionDof(const unsigned &i, const unsigned &iel, const short unsigned &solType) const {

    unsigned dof;
    switch(solType){
      case 0: // linear Lagrange
	{
	  unsigned iNode = el->GetMeshDof(iel, i, solType);
	  unsigned isdom = IsdomBisectionSearch(iNode, 2);
	  if(iNode < _dofOffset[2][isdom]+_originalOwnSize[0][isdom]){
	    dof = (iNode - _dofOffset[2][isdom]) + _dofOffset[0][isdom];
	  }
	  else{
	    dof = _ownedGhostMap[0].find(iNode)->second;
	  }
	}
	break;
      case 1: // quadratic Lagrange
       	{
	  unsigned iNode = el->GetMeshDof(iel, i, solType);
	  unsigned isdom = IsdomBisectionSearch(iNode, 2);
	  if(iNode < _dofOffset[2][isdom]+_originalOwnSize[1][isdom]){
	    dof = (iNode - _dofOffset[2][isdom]) + _dofOffset[1][isdom];
	  }
	  else{
	    dof = _ownedGhostMap[1].find(iNode)->second;
	  }
	}
	break;

      case 2: // bi-quadratic Lagrange
        dof = el->GetMeshDof(iel, i, solType);
        break;
      case 3: // piecewise constant
	// in this case use i=0
        dof = iel;
        break;
      case 4: // piecewise linear discontinuous
	unsigned isdom = IsdomBisectionSearch(iel, 3);
	unsigned offset = _elementOffset[isdom];
        unsigned offsetp1 = _elementOffset[isdom + 1];
        unsigned ownSize = offsetp1 - offset;
        unsigned offsetPWLD = offset * (_dimension + 1);
        unsigned locIel = iel - offset;
        dof = offsetPWLD + ( i * ownSize ) + locIel;
        break;
      }
    return dof;
  }

  // *******************************************************

SparseMatrix* Mesh::GetQitoQjProjection(const unsigned& itype, const unsigned& jtype) {
  if(itype < 3 && jtype < 3){
    if(!_ProjQitoQj[itype][jtype]){
      BuildQitoQjProjection(itype, jtype);
    }
  }
  else{
    std::cout<<"Wrong argument range in function"
	     <<"Mesh::GetLagrangeProjectionMatrix(const unsigned& itype, const unsigned& jtype)"<<std::cout;
    abort();
  }
  return _ProjQitoQj[itype][jtype];
}

void Mesh::BuildQitoQjProjection(const unsigned& itype, const unsigned& jtype){

  unsigned ni = _dofOffset[itype][_nprocs];
  unsigned ni_loc = _ownSize[itype][_iproc];

  unsigned nj = _dofOffset[jtype][_nprocs];
  unsigned nj_loc = _ownSize[itype][_iproc];

  NumericVector *NNZ_d = NumericVector::build().release();
  if(1 == _nprocs) { // IF SERIAL
    NNZ_d->init(ni, ni_loc, false, SERIAL);
  }
  else{
    NNZ_d->init(ni, ni_loc, _ghostDofs[itype][processor_id()], false, GHOSTED);
  }
  NNZ_d->zero();

  NumericVector *NNZ_o = NumericVector::build().release();
  NNZ_o->init(*NNZ_d);
  NNZ_o->zero();

  for(unsigned isdom = _iproc; isdom < _iproc+1; isdom++) {
    for (unsigned iel = _elementOffset[isdom]; iel < _elementOffset[isdom+1]; iel++){
      short unsigned ielt = GetElementType(iel);
      _finiteElement[ielt][jtype]->GetSparsityPatternSize(*this, iel, NNZ_d, NNZ_o, itype);
    }
  }

  NNZ_d->close();
  NNZ_o->close();

  unsigned offset = _dofOffset[itype][_iproc];

  vector < int > nnz_d(ni_loc);
  vector < int > nnz_o(ni_loc);
  for(unsigned i = 0; i < ni_loc; i++){
    nnz_d[i] = static_cast < int > ((*NNZ_d)(offset+i));
    nnz_o[i] = static_cast < int > ((*NNZ_o)(offset+i));
  }

  _ProjQitoQj[itype][jtype] = SparseMatrix::build().release();
  _ProjQitoQj[itype][jtype]->init(ni, nj, _ownSize[itype][_iproc], _ownSize[jtype][_iproc], nnz_d, nnz_o);
  for(unsigned isdom = _iproc; isdom < _iproc+1; isdom++) {
    for (unsigned iel = _elementOffset[isdom]; iel < _elementOffset[isdom+1]; iel++){
      short unsigned ielt = GetElementType(iel);
      _finiteElement[ielt][jtype]->BuildProlongation(*this, iel, _ProjQitoQj[itype][jtype], NNZ_d, NNZ_o,itype);
    }
  }
  _ProjQitoQj[itype][jtype]->close();

  delete NNZ_d;
  delete NNZ_o;
}



SparseMatrix* Mesh::GetCoarseToFineProjection(const unsigned& solType){

  if( solType >= 5 ){
    std::cout<<"Wrong argument range in function \"GetCoarseToFineProjection\": "
	     <<"solType is greater then SolTypeMax"<<std::endl;
    abort();
  }

  if(!_ProjCoarseToFine[solType])
    BuildCoarseToFineProjection(solType);

  return _ProjCoarseToFine[solType];
}



void Mesh::BuildCoarseToFineProjection(const unsigned& solType){

  if (!_coarseMsh) {
    std::cout<<"Error! In function \"BuildCoarseToFineProjection\": the coarse mesh has not been set"<<std::endl;
    abort();
  }

  if( !_ProjCoarseToFine[solType] ){

    int nf     = _dofOffset[solType][_nprocs];
    int nc     = _coarseMsh->_dofOffset[solType][_nprocs];
    int nf_loc = _ownSize[solType][_iproc];
    int nc_loc = _coarseMsh->_ownSize[solType][_iproc];

    //build matrix sparsity pattern size
    NumericVector *NNZ_d = NumericVector::build().release();
    if(n_processors()==1) { // IF SERIAL
      NNZ_d->init(nf, nf_loc, false, SERIAL);
    }
    else { // IF PARALLEL
      if(solType<3) { // GHOST nodes only for Lagrange FE families
	NNZ_d->init(nf, nf_loc, _ghostDofs[solType][processor_id()], false, GHOSTED);
      }
      else { //piecewise discontinuous variables have no ghost nodes
	NNZ_d->init(nf, nf_loc, false, PARALLEL);
      }
    }
    NNZ_d->zero();

    NumericVector *NNZ_o = NumericVector::build().release();
    NNZ_o->init(*NNZ_d);
    NNZ_o->zero();

    for(int isdom=_iproc; isdom<_iproc+1; isdom++) {
      for (int iel = _coarseMsh->_elementOffset[isdom];iel < _coarseMsh->_elementOffset[isdom+1]; iel++) {
	short unsigned ielt=_coarseMsh->GetElementType(iel);
	_finiteElement[ielt][solType]->GetSparsityPatternSize( *this, *_coarseMsh, iel, NNZ_d, NNZ_o);
      }
    }
    NNZ_d->close();
    NNZ_o->close();

    unsigned offset = _dofOffset[solType][_iproc];
    vector <int> nnz_d(nf_loc);
    vector <int> nnz_o(nf_loc);
    for(int i=0; i<nf_loc;i++){
      nnz_d[i]=static_cast <int> (floor((*NNZ_d)(offset+i)+0.5));
      nnz_o[i]=static_cast <int> (floor((*NNZ_o)(offset+i)+0.5));
    }
    delete NNZ_d;
    delete NNZ_o;

    //build matrix
    _ProjCoarseToFine[solType] = SparseMatrix::build().release();
    _ProjCoarseToFine[solType]->init(nf,nc,nf_loc,nc_loc,nnz_d,nnz_o);

    // loop on the coarse grid
    for(int isdom=_iproc; isdom<_iproc+1; isdom++) {
      for (int iel=_coarseMsh->_elementOffset[isdom]; iel < _coarseMsh->_elementOffset[isdom+1]; iel++) {
        short unsigned ielt=_coarseMsh->GetElementType(iel);
	_finiteElement[ielt][solType]->BuildProlongation(*this, *_coarseMsh,iel, _ProjCoarseToFine[solType]);
      }
    }
    _ProjCoarseToFine[solType]->close();
  }
}


short unsigned Mesh::GetRefinedElementIndex(const unsigned &iel) const{
  return static_cast <short unsigned> ( (*_topology->_Sol[_amrIndex])(iel) + 0.5);
}

short unsigned Mesh::GetElementGroup(const unsigned int& iel) const{
  return static_cast <short unsigned> ( (*_topology->_Sol[_groupIndex])(iel) + 0.5);
}

short unsigned Mesh::GetElementMaterial(const unsigned int& iel) const{
  return static_cast <short unsigned> ( (*_topology->_Sol[_materialIndex])(iel) + 0.5);
}

short unsigned Mesh::GetElementType(const unsigned int& iel) const{
  return static_cast <short unsigned> ( (*_topology->_Sol[_typeIndex])(iel) + 0.5);
}


} //end namespace femus


