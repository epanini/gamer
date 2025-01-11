#include "GAMER.h"

//-------------------------------------------------------------------------------------------------------
// Function    :  Aux_Record_Performance
// Description :  Record the code performance
//
// Note        :  1. The code performance is defined as "total number of cell updates per second"
//                   --> Note that for the individual time-step integration cells at higher levels will be updated
//                       more frequently
//                   --> The total number of cell and particle updates recorded here for the individual time-step
//                       integration is only approximate since the number of patches at each level may change
//                       during one global time-step
//                2. When PARTICLE is on, this routine also records the "total number of particle updates per second"
//                3. Calculates and records the average performance (Ncell/sec and Ncell/sec/MPIrank) at the end
//                   of the simulation.
//
// Parameter   :  ElapsedTime : Elapsed time of the current global step
//-------------------------------------------------------------------------------------------------------
void Aux_Record_Performance( const double ElapsedTime )
{
   const char FileName[] = "Record__Performance";
   static bool FirstTime = true;

   // Accumulate total updates and time for average performance
   static long Total_NUpdateCell = 0;
   static double Total_ElapsedTime = 0.0;
   static double Total_NUpdateCell_PerSec_PerRank = 0.0;

#  ifdef PARTICLE
   long NPar_Lv_AllRank[NLEVEL];
   MPI_Reduce( amr->Par->NPar_Lv, NPar_Lv_AllRank, NLEVEL, MPI_LONG, MPI_SUM, 0, MPI_COMM_WORLD );
#  endif

   if ( MPI_Rank == 0 )
   {
      // Header
      if ( FirstTime )
      {
         if ( Aux_CheckFileExist(FileName) )
            Aux_Message( stderr, "WARNING : file \"%s\" already exists !!\n", FileName );

         FirstTime = false;

         FILE *File_Record = fopen( FileName, "a" );

         fprintf( File_Record, "#%13s%14s%3s%14s%14s%14s%14s%14s%14s",
                  "Time", "Step", "", "dt", "NCell", "NUpdate_Cell", "ElapsedTime", "Perf_Overall", "Perf_PerRank" );
#        ifdef PARTICLE
         fprintf( File_Record, "%14s%14s%17s%17s",
                  "NParticle", "NUpdate_Par", "ParPerf_Overall", "ParPerf_PerRank" );
#        endif

         for (int lv=0; lv<NLEVEL; lv++)
         {
            char tmp[MAX_STRING];
            sprintf( tmp, "NUpdate_Lv%d", lv );
            fprintf( File_Record, "%14s", tmp );
         }

         fprintf( File_Record, "\n" );
         fclose( File_Record );
      }

      // Count total number of cells, updates, and particles
      long NCell = 0, NUpdateCell = 0, NCellThisLevel;
#     ifdef PARTICLE
      long NUpdatePar = 0;
#     endif

      for (int lv=0; lv<NLEVEL; lv++)
      {
         NCellThisLevel = (long)NPatchTotal[lv]*CUBE( PATCH_SIZE );
         NCell         += NCellThisLevel;
         NUpdateCell   += NCellThisLevel     *amr->NUpdateLv[lv];
#        ifdef PARTICLE
         NUpdatePar    += NPar_Lv_AllRank[lv]*amr->NUpdateLv[lv];
#        endif
      }

      // Update totals for average performance
      Total_NUpdateCell += NUpdateCell;
      Total_ElapsedTime += ElapsedTime;

      const double NUpdateCell_PerSec = NUpdateCell / ElapsedTime;
      const double NUpdateCell_PerSec_PerRank = NUpdateCell_PerSec / MPI_NRank;

      Total_NUpdateCell_PerSec_PerRank += NUpdateCell_PerSec_PerRank;

#     ifdef PARTICLE
      const double NUpdatePar_PerSec = NUpdatePar / ElapsedTime;
      const double NUpdatePar_PerSec_PerRank = NUpdatePar_PerSec / MPI_NRank;
#     endif

      // Record performance
      FILE *File_Record = fopen( FileName, "a" );

      fprintf( File_Record, "%14.7e%14ld%3s%14.7e%14.2e%14.2e%14.2e%14.2e%14.2e",
               Time[0], Step, "", dTime_Base, (double)NCell, (double)NUpdateCell, ElapsedTime, NUpdateCell_PerSec,
               NUpdateCell_PerSec_PerRank );

#     ifdef PARTICLE
      fprintf( File_Record, "%14.2e%14.2e%17.2e%17.2e",
               (double)amr->Par->NPar_Active_AllRank, (double)NUpdatePar, NUpdatePar_PerSec, NUpdatePar_PerSec_PerRank );
#     endif

      for (int lv=0; lv<NLEVEL; lv++)
         fprintf( File_Record, "%14ld", amr->NUpdateLv[lv] );

      fprintf( File_Record, "\n" );
      fclose( File_Record );

      // At the end of the simulation, calculate and record average performance
      if (  Step == END_STEP || Time[0] >= END_T  ) // Replace with the actual condition for end of simulation
      {
         const double Avg_NUpdateCell_PerSec = Total_NUpdateCell / Total_ElapsedTime;
         const double Avg_NUpdateCell_PerSec_PerRank = Total_NUpdateCell_PerSec_PerRank / Step;

         File_Record = fopen( FileName, "a" );
         fprintf( File_Record, "\n# Average performance over the entire simulation:\n");
         fprintf( File_Record, "#%14s%14s%14s\n", "TotalTime", "AvgPerf_Overall", "AvgPerf_PerRank");
         fprintf( File_Record, "%14.7e%14.2e%14.2e\n", Total_ElapsedTime, Avg_NUpdateCell_PerSec, Avg_NUpdateCell_PerSec_PerRank);
         fclose( File_Record );
      }
   }
}
