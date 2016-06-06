#include "Copyright.h"
#include "GAMER.h"

#ifdef PARTICLE




//-------------------------------------------------------------------------------------------------------
// Function    :  Par_GetTimeStep_VelAcc
// Description :  Estimate the evolution time-step and physical time interval by the maximum particle velocity
//                and acceleration
//                --> dt_vel = DT__PARVEL*dh/v_max, where v_max = max(vx,vy,vz,all_particles)
//                    dt_acc = DT__PARACC*(dh/a_max)^0.5, where a_max = max(accx,accy,accz,all_particles)
//
// Note        :  1. Physical coordinates : dTime == dt
//                   Comoving coordinates : dTime == dt*(Hubble parameter)*(scale factor)^3 == delta(scale factor)
//                2. Particle acceleration criterion is used only when DT__PARACC > 0.0
// 
// Parameter   :  dt       : Time interval to advance solution
//                dTime    : Time interval to update physical time 
//                MinDtLv  : Refinement level determining the smallest time-step
//                MinDtVar : Maximum velocity and acceleration determining the minimum time-step
//                           [0/1] ==> [velocity/acceleration]
//                dt_dTime : dt/dTime (== 1.0 if COMOVING is off)
//
// Return      :  dt, dTime, MinDtLv, MinDtVar
//-------------------------------------------------------------------------------------------------------
void Par_GetTimeStep_VelAcc( double dt[2], double dTime[2], int MinDtLv[2], real MinDtVar[2], const double dt_dTime )
{

   const real *Vel[3] = { amr->Par->VelX, amr->Par->VelY, amr->Par->VelZ };
#  ifdef STORE_PAR_ACC
   const real *Acc[3] = { amr->Par->AccX, amr->Par->AccY, amr->Par->AccZ };
#  else
   const real *Acc[3] = { NULL, NULL, NULL };
#  endif
   const bool  UseAcc = ( DT__PARACC > 0.0 );

#  ifndef STORE_PAR_ACC
   if ( UseAcc )
      Aux_Error( ERROR_INFO, "DT__PARACC (%14.7e) > 0.0 when STORE_PAR_ACC is off !!\n", DT__PARACC );
#  endif

   real  *MaxVel      = MinDtInfo_ParVelAcc[0];       // "MinDtInfo_ParVelAcc" is a global variable
   real  *MaxAcc      = MinDtInfo_ParVelAcc[1];
   double dt_local[2] = { __FLT_MAX__, __FLT_MAX__ }; // initialize as extremely large numbers
   double dt_min[2], dt_tmp[2];
   long   ParID;


// get the maximum particle velocity and acceleration at each level
   if ( !OPT__ADAPTIVE_DT )
   {
      for (int lv=0; lv<NLEVEL; lv++)
      {
         MaxVel[lv] = 0.0;    // don't assign negative values since we assume it to be positive-definite
         MaxAcc[lv] = 0.0;

         for (int PID=0; PID<amr->NPatchComma[lv][1]; PID++)
         for (int p=0; p<amr->patch[0][lv][PID]->NPar; p++)
         {
            ParID = amr->patch[0][lv][PID]->ParList[p];

            for (int d=0; d<3; d++)
            MaxVel[lv] = MAX( MaxVel[lv], FABS(Vel[d][ParID]) );

            if ( UseAcc )
            for (int d=0; d<3; d++)
            MaxAcc[lv] = MAX( MaxAcc[lv], FABS(Acc[d][ParID]) );
         }
      }
   }


// get the time-step in one rank
   MinDtLv[0] = -1;  // indicating that MinDtVar cannot be obtained
   MinDtLv[1] = -1;

   for (int lv=0; lv<NLEVEL; lv++)
   {
      dt_tmp[0]  =       amr->dh[lv] / MaxVel[lv];
      if ( UseAcc )
      dt_tmp[1]  = sqrt( amr->dh[lv] / MaxAcc[lv] );

//    return 2*dt for the individual time-step since at the base level each step actually includes two sub-steps
#     ifdef INDIVIDUAL_TIMESTEP
      dt_tmp[0] *= double( 1<<(lv+1) );
      if ( UseAcc )
      dt_tmp[1] *= double( 1<<(lv+1) );
#     endif

      if ( dt_tmp[0] < dt_local[0] )    
      {
         dt_local[0] = dt_tmp[0];
         MinDtLv [0] = lv;
         MinDtVar[0] = MaxVel[lv];
      }

      if ( UseAcc )
      if ( dt_tmp[1] < dt_local[1] )    
      {
         dt_local[1] = dt_tmp[1];
         MinDtLv [1] = lv;
         MinDtVar[1] = MaxAcc[lv];
      }
   } // for (int lv=0; lv<NLEVEL; lv++)


// get the minimum time-step from all ranks
   MPI_Allreduce( &dt_local[0], &dt_min[0], 1, MPI_DOUBLE, MPI_MIN, MPI_COMM_WORLD );

   if ( UseAcc )
   MPI_Allreduce( &dt_local[1], &dt_min[1], 1, MPI_DOUBLE, MPI_MIN, MPI_COMM_WORLD );


// verify the minimum time-step
   if ( dt_min[0] == __FLT_MAX__  &&  amr->Par->NPar_Active > 0 )
   {
      Aux_Message( stderr, "WARNING : time-step estimation by particle velocity is incorrect (dt_min = %13.7e) !!\n", dt_min[0] );
      Aux_Message( stderr, "          --> Likely all particles have zero velocity\n" );

      if ( DT__PARVEL_MAX < 0.0 )
      Aux_Message( stderr, "          --> You might want to set DT__PARVEL_MAX properly\n" );
   }

   if ( UseAcc  &&  dt_min[1] == __FLT_MAX__  &&  amr->Par->NPar_Active > 0 )
      Aux_Error( ERROR_INFO, "time-step estimation by particle acceleration is incorrect (dt_min = %13.7e) !!\n", dt_min[1] );



// gather the minimum time-step information from all ranks
   /*
#  ifndef SERIAL
   double *dt_AllRank     = new double [MPI_NRank];
   int    *MinDtLv_AllRank  = new int    [MPI_NRank];
   real   *MinDtVar_AllRank = new real   [MPI_NRank];
   
   MPI_Gather( &dt_local, 1, MPI_DOUBLE, dt_AllRank,       1, MPI_DOUBLE, 0, MPI_COMM_WORLD );
   MPI_Gather( &MinDtLv,  1, MPI_INT,    MinDtLv_AllRank,  1, MPI_INT,    0, MPI_COMM_WORLD );
#  ifdef FLOAT8
   MPI_Gather( &MinDtVar, 1, MPI_DOUBLE, MinDtVar_AllRank, 1, MPI_DOUBLE, 0, MPI_COMM_WORLD );
#  else
   MPI_Gather( &MinDtVar, 1, MPI_FLOAT,  MinDtVar_AllRank, 1, MPI_FLOAT,  0, MPI_COMM_WORLD );
#  endif

   if ( MPI_Rank == 0 )
   {
      for (int Rank=0; Rank<MPI_NRank; Rank++)
      {
         if ( dt_AllRank[Rank] == dt_min )
         {
            MinDtLv  = MinDtLv_AllRank [Rank];
            MinDtVar = MinDtVar_AllRank[Rank];
            break;
         }

         if ( Rank == MPI_NRank-1 )    Aux_Message( stderr, "WARNING : no match of \"dt_min\" was found !!\n" );
      }
   }

   delete [] dt_AllRank;
   delete [] MinDtLv_AllRank;
   delete [] MinDtVar_AllRank;
#  endif // #ifndef SERIAL 
   */

#  ifndef SERIAL
#  error : ERROR : only SERIAL work here
#  endif

#  ifdef COMOVING
#  error : ERROR : COMOVING needs to be checked here
#  endif


   dt[0] = DT__PARVEL * dt_min[0];
   if ( DT__PARVEL_MAX >= 0.0 )  dt[0] = MIN( dt[0], DT__PARVEL_MAX );
   dTime[0] = dt[0] / dt_dTime;

   if ( UseAcc )
   {
      dt[1] = DT__PARACC * dt_min[1];
      dTime[1] = dt[1] / dt_dTime;
   }

} // FUNCTION : Par_GetTimeStep_VelAcc



#endif // #ifdef PARTICLE
