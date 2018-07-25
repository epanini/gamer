#include "GAMER.h"

#ifdef SUPPORT_GRACKLE


// global variables for accessing h_Che_Array[]
// --> declared in Init_MemAllocate_Grackle.cpp
extern int Che_NField;
extern int CheIdx_Dens;
extern int CheIdx_sEint;
extern int CheIdx_Ek;
extern int CheIdx_e;
extern int CheIdx_HI;
extern int CheIdx_HII;
extern int CheIdx_HeI;
extern int CheIdx_HeII;
extern int CheIdx_HeIII;
extern int CheIdx_HM;
extern int CheIdx_H2I;
extern int CheIdx_H2II;
extern int CheIdx_DI;
extern int CheIdx_DII;
extern int CheIdx_HDI;
extern int CheIdx_Metal;




//-------------------------------------------------------------------------------------------------------
// Function    :  Grackle_Close
// Description :  Copy the specific internal energy updated by the Grackle solver back to the
//                patch pointers
//
// Note        :  1. Use SaveSg to determine where to store the data
//                   --> Currently it's set to the same Sg as the fluid data when calling
//                       Grackle_AdvanceDt() in EvolveLevel()
//                2. Che_NField and the corresponding array indices in h_Che_Array[] (e.g., CheIdx_Dens)
//                   are declared and set by Init_MemAllocate_Grackle()
//
// Parameter   :  lv          : Target refinement level
//                SaveSg      : Sandglass to store the updated data
//                h_Che_Array : Host array storing the updated data
//                NPG         : Number of patch groups to store the updated data
//                PID0_List   : List recording the patch indicies with LocalID==0 to be udpated
//-------------------------------------------------------------------------------------------------------
void Grackle_Close( const int lv, const int SaveSg, const real h_Che_Array[], const int NPG, const int *PID0_List )
{

   const int   Size1pg  = CUBE(PS2);
   const int   Size1v   = NPG*Size1pg;
   const real  Gamma_m1 = GAMMA - (real)1.0;
   const real _Gamma_m1 = (real)1.0 / Gamma_m1;

   const real *Ptr_Dens0  = h_Che_Array + CheIdx_Dens *Size1v;
   const real *Ptr_sEint0 = h_Che_Array + CheIdx_sEint*Size1v;
   const real *Ptr_Ek0    = h_Che_Array + CheIdx_Ek   *Size1v;


#  pragma omp parallel
   {

// thread-private variables
   int  idx_pg, PID, PID0, offset;  // idx_pg: array indices within a patch group
   real Dens, Pres;

   const real *Ptr_Dens=NULL, *Ptr_sEint=NULL, *Ptr_Ek=NULL;

#  pragma omp for schedule( static )
   for (int TID=0; TID<NPG; TID++)
   {
      PID0      = PID0_List[TID];
      idx_pg    = 0;
      offset    = TID*Size1pg;

      Ptr_Dens  = Ptr_Dens0  + offset;
      Ptr_sEint = Ptr_sEint0 + offset;
      Ptr_Ek    = Ptr_Ek0    + offset;

      for (int LocalID=0; LocalID<8; LocalID++)
      {
         PID = PID0 + LocalID;

         for (int idx_p=0; idx_p<CUBE(PS1); idx_p++)
         {
//          apply the minimum pressure check
            Dens = Ptr_Dens [idx_pg];
            Pres = Ptr_sEint[idx_pg]*Dens*Gamma_m1;
            Pres = CPU_CheckMinPres( Pres, MIN_PRES );

//          update the total energy density
            *( amr->patch[SaveSg][lv][PID]->fluid[ENGY][0][0] + idx_p ) = Pres*_Gamma_m1 + Ptr_Ek[idx_pg];

//          update the dual-energy variable to be consistent with the updated pressure
#           ifdef DUAL_ENERGY
#           if   ( DUAL_ENERGY == DE_ENPY )
            *( amr->patch[SaveSg][lv][PID]->fluid[ENPY][0][0] + idx_p ) = CPU_DensPres2Entropy( Dens, Pres, Gamma_m1 );

#           elif ( DUAL_ENERGY == DE_EINT )
#           error : DE_EINT is NOT supported yet !!
#           endif
#           endif // #ifdef DUAL_ENERGY

            idx_pg ++;
         } // for (int idx_p=0; idx_p<CUBE(PS1); idx_p++)
      } // for (int LocalID=0; LocalID<8; LocalID++)
   } // for (int TID=0; TID<NPG; TID++)

   } // end of OpenMP parallel region

} // FUNCTION : Grackle_Close



#endif // #ifdef SUPPORT_GRACKLE
