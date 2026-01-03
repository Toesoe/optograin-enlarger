/**
 * @file cli.h
 * @brief Command-line interface over RTT for testing
 */

#ifndef _CLI_H_
#define _CLI_H_

#ifdef __cplusplus
extern "C"
{
#endif

//=====================================================================================================================
// Functions
//=====================================================================================================================

void cliInit(void);
void cliProcess(void);

#ifdef __cplusplus
}
#endif
#endif //!_CLI_H_
