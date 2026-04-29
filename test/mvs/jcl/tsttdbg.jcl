//TSTTDBG  JOB (A),'CLOCK DIAG',REGION=4M,
//           CLASS=A,MSGCLASS=H,MSGLEVEL=(1,1),
//           NOTIFY=IBMUSER
//*---------------------------------------------------------------
//* TSTTDBG - WP-CPS-01 clock / gmtime64 diagnostic.
//*
//* Prints raw clock64/uclock64 words, usec fraction, and struct tm
//* output for three time64_t init variants. Run after tsttim fails
//* on DATE('S') or TIME('L') to isolate the root cause on this MVS.
//*
//* Expected: step RC=0; JESLOG shows all three DATE('S') values
//*   as 'yyyymmdd' (len=8, OK) and usec fraction 'OK (0..999999)'.
//*
//* From TSO: CALL 'IBMUSER.REXX370.V0R1M0D.LOAD(TSTTDBG)'
//*---------------------------------------------------------------
//STEP1   EXEC PGM=TSTTDBG,REGION=4M
//STEPLIB  DD DSN=IBMUSER.REXX370.V0R1M0D.LOAD,DISP=SHR
//SYSPRINT DD SYSOUT=*
//
