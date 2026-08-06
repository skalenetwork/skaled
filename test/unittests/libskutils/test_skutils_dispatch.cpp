#include "test_skutils_helper.h"
#include <test/tools/libtesteth/TestHelper.h>
#include <boost/test/unit_test.hpp>

static const bool g_bShowDetailedJobLogs = false;  // useful for dispatch development only

static std::string thread_prefix_str() {
    std::stringstream ssThreadID;
    ssThreadID << std::this_thread::get_id();
    std::string strThreadID = ssThreadID.str();
    static const size_t g_nMaxThreadIdLen = 6;
    size_t nLen = strThreadID.length();
    if ( nLen > g_nMaxThreadIdLen )
        strThreadID = strThreadID.substr( nLen - g_nMaxThreadIdLen );
    //
    std::stringstream ssLoop;
    std::string strLoop;
    skutils::dispatch::loop_ptr_t pLoop = skutils::dispatch::loop::get_current();
    if ( pLoop ) {
        ssLoop << pLoop;
        strLoop = ssLoop.str();
        static const size_t g_nMaxLoopIdLen = 6;
        size_t nLen = strLoop.length();
        if ( nLen > g_nMaxLoopIdLen )
            strLoop = strLoop.substr( nLen - g_nMaxLoopIdLen );
    } else
        strLoop = "~";
    //
    std::string strQueue;
    skutils::dispatch::queue_ptr_t pQueue = skutils::dispatch::queue::get_current();
    if ( pQueue ) {
        std::stringstream ssQueue;
        ssQueue << pQueue;
        strQueue = ssQueue.str();
        static const size_t g_nMaxQueueIdLen = 6;
        size_t nLen = strQueue.length();
        if ( nLen > g_nMaxQueueIdLen )
            strQueue = strQueue.substr( nLen - g_nMaxQueueIdLen );
        std::string strID = pQueue->get_id();
        if ( !strID.empty() )
            strQueue += "-" + strID;
    } else
        strQueue = "~";
    //
    std::stringstream ssDomain;
    std::string strDomain;
    skutils::dispatch::domain_ptr_t pDomain = skutils::dispatch::domain::get_current();
    if ( pDomain ) {
        ssDomain << pDomain;
        strDomain = ssDomain.str();
        static const size_t g_nMaxDomainIdLen = 6;
        size_t nLen = strDomain.length();
        if ( nLen > g_nMaxDomainIdLen )
            strDomain = strDomain.substr( nLen - g_nMaxDomainIdLen );
    } else
        strDomain = "~";
    // thread / loop / queue / domain
    return "TLQD/" + strThreadID + "/" + strLoop + "/" + strQueue + "/" + strDomain + ": ";
}

BOOST_AUTO_TEST_SUITE( SkUtils )
BOOST_AUTO_TEST_SUITE( dispatch, *boost::unit_test::precondition( dev::test::option_all_tests ) )

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

BOOST_AUTO_TEST_CASE( loop_functionality_alive ) {
    skutils::test::test_print_header_name( "SkUtils/dispatch/loop_functionality_alive" );
    skutils::test::with_test_environment( [&]() {
        skutils::test::test_log_e( thread_prefix_str() + "creating loop instance..." );
        skutils::dispatch::loop_ptr_t pLoop( new skutils::dispatch::loop );
        pLoop->on_job_will_add_ = [&]( const skutils::dispatch::job_id_t& id ) -> bool {
            skutils::test::test_log_e( thread_prefix_str() + "will add job " + id );
            return true;
        };
        pLoop->on_job_was_added_ = [&]( const skutils::dispatch::job_id_t& id ) -> void {
            skutils::test::test_log_e( thread_prefix_str() + "did added job " + id );
        };
        pLoop->on_job_will_remove_ = [&]( const skutils::dispatch::job_id_t& id ) -> bool {
            skutils::test::test_log_e( thread_prefix_str() + "will remove job " + id );
            return true;
        };
        pLoop->on_job_did_removed_ = [&]( const skutils::dispatch::job_id_t& id ) -> void {
            skutils::test::test_log_e( thread_prefix_str() + "did removed job " + id );
        };
        pLoop->on_job_will_execute_ = [&]( const skutils::dispatch::job_id_t& id ) -> bool {
            skutils::test::test_log_e( thread_prefix_str() + "will execute job " + id );
            return true;
        };
        pLoop->on_job_did_executed_ = [&]( const skutils::dispatch::job_id_t& id ) -> void {
            skutils::test::test_log_e( thread_prefix_str() + "did executed job " + id );
        };
        pLoop->on_job_exception_ = [&]( const skutils::dispatch::job_id_t& id,
                                       std::exception* pe ) -> void {
            skutils::test::test_log_e(
                thread_prefix_str() + "exception in job " + id +
                ", exception info: " + ( pe ? pe->what() : "unknown exception" ) );
        };
        //
        skutils::test::test_log_e( thread_prefix_str() + "starting loop thread..." );
        std::thread t( [&]() -> void {
            skutils::test::test_log_e( thread_prefix_str() + "will run loop in thread..." );
            pLoop->run();
            skutils::test::test_log_e( thread_prefix_str() + "will exit loop thread..." );
        } );
        skutils::test::test_log_e( thread_prefix_str() + "waiting loop to start finished..." );
        pLoop->wait_until_startup();
        //
        static const size_t nSleepSeconds = 3;
        //
        size_t nCallCountOnce = 0;
        const uint64_t nOnceJobTimeout = 500;  // milliseconds
        pLoop->job_add_once(
            "once uppon a time",
            [&]() -> void {
                ++nCallCountOnce;
                skutils::test::test_log_e( thread_prefix_str() +
                                           "--- once uppon a time job, invocation " +
                                           std::to_string( nCallCountOnce - 1 ) );
            },
            skutils::dispatch::duration_from_milliseconds( nOnceJobTimeout ) );
        //
        pLoop->job_add_once(
            "bad job",
            [&]() -> void {
                skutils::test::test_log_e(
                    thread_prefix_str() + "bad job invoked, throwing someting" );
                // throw 12345;
                throw std::runtime_error( "exception thrown from bad job" );
            },
            skutils::dispatch::duration_from_milliseconds( 1200 )  // 1.2 seconds
        );
        //
        size_t nCallCountPeriodical = 0;
        const uint64_t nPeriodicJobTimeout = 500;  // milliseconds
        const size_t nExpectedCallCountPeriodical =
            ( nSleepSeconds * 1000 ) / nPeriodicJobTimeout - 1;  // -1 for safety)
        skutils::test::test_log_e( thread_prefix_str() + "expecting periodical job to be invoked " +
                                   std::to_string( nExpectedCallCountPeriodical ) +
                                   " time(s), at least" );
        pLoop->job_add_periodic(
            "some periodical work",
            [&]() -> void {
                ++nCallCountPeriodical;
                skutils::test::test_log_e( thread_prefix_str() + "--- periodical job, invocation " +
                                           std::to_string( nCallCountPeriodical - 1 ) );
            },
            skutils::dispatch::duration_from_milliseconds( nPeriodicJobTimeout ) );
        //
        skutils::test::test_log_e( thread_prefix_str() + "will sleep " +
                                   std::to_string( nSleepSeconds ) + " second(s)..." );
        sleep( nSleepSeconds );
        //
        skutils::test::test_log_e( thread_prefix_str() + "will cancel loop..." );
        pLoop->cancel();
        skutils::test::test_log_e( thread_prefix_str() + "will wait for loop..." );
        pLoop->wait();
        try {
            if ( t.joinable() ) {
                skutils::test::test_log_e( thread_prefix_str() + "will wait for loop thread..." );
                t.join();
            }
        } catch ( ... ) {
        }
        BOOST_REQUIRE( nCallCountOnce == 1 );
        BOOST_REQUIRE( nCallCountPeriodical >=
                       nExpectedCallCountPeriodical );  // some number of calls should be performed
        skutils::test::test_log_e( thread_prefix_str() + "end of loop test" );
    } );
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

BOOST_AUTO_TEST_CASE( domain_functionality_alive ) {
    skutils::test::test_print_header_name( "SkUtils/dispatch/domain_functionality_alive" );
    skutils::test::with_test_environment( [&]() {
        static const size_t nExpectedCallCount = 50;
        std::atomic_size_t nCallCounter( 0 );
        {  // block for domain
            skutils::test::test_log_e( thread_prefix_str() + "creating domain instance..." );
            skutils::dispatch::domain_ptr_t pDomain( new skutils::dispatch::domain );

            size_t i;
            std::atomic_bool bInsideCall( false );
            skutils::test::test_log_e( thread_prefix_str() + "expecting async job to be invoked " +
                                       std::to_string( nExpectedCallCount ) +
                                       " time(s), at least" );
            for ( i = 0; i < nExpectedCallCount; ++i ) {
                skutils::dispatch::queue_ptr_t pQueue(
                    pDomain->queue_get( skutils::dispatch::get_default_queue_id(), true ) );
                pQueue->job_add( [&]() {
                    BOOST_REQUIRE( !bool( bInsideCall ) );
                    bInsideCall = true;
                    ++nCallCounter;
                    if ( g_bShowDetailedJobLogs )
                        skutils::test::test_log_e( thread_prefix_str() +
                                                   "--- async job in queue, invocation " +
                                                   std::to_string( size_t( nCallCounter ) - 1 ) );
                    BOOST_REQUIRE( bool( bInsideCall ) );
                    bInsideCall = false;
                } );
            }
            //
            //
            static const size_t nSleepSeconds = 5, nWaitRoundCount = 5;
            for ( size_t nWaitRound = 0; nWaitRound < nWaitRoundCount; ++nWaitRound ) {
                skutils::test::test_log_e(
                    thread_prefix_str() + "waiting for test to complete in round " +
                    std::to_string( nWaitRound + 1 ) + " of " + std::to_string( nWaitRoundCount ) +
                    ", will sleep " + std::to_string( nSleepSeconds ) + " second(s)..." );
                sleep( nSleepSeconds );
                skutils::test::test_log_e( thread_prefix_str() + "done sleeping " +
                                           std::to_string( nSleepSeconds ) +
                                           " second(s), end of domain life time..." );
                if ( size_t( nExpectedCallCount ) == size_t( nCallCounter ) )
                    break;
            }  // for( size_t nWaitRound = 0; nWaitRound < nWaitRoundCount; ++ nWaitRound )
            //
            //
            skutils::test::test_log_e( thread_prefix_str() + "shutting down domain..." );
            pDomain->shutdown();
        }  // block for domain
        BOOST_REQUIRE( nExpectedCallCount == nCallCounter );
        //
        skutils::test::test_log_e( thread_prefix_str() + "end of domain alive test" );
    } );
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

BOOST_AUTO_TEST_CASE( job_priorities_alive ) {
    skutils::test::test_print_header_name( "SkUtils/dispatch/job_priorities_alive" );
    skutils::test::with_test_environment( [&]() {
        size_t g_arrThreadCounts[] = { 1, 2, 4 /*, 8, 16, 32, 64*/ };  // tests with different count
                                                                       // of threads
        for ( const size_t& nThreadCount : g_arrThreadCounts ) {
            skutils::test::test_log_e(
                "# "
                "-----------------------------------------------------------------------" );
            skutils::test::test_log_e(
                "# "
                "job_priorities_alive with nThreadCount=" +
                nThreadCount );
            skutils::test::test_log_e(
                "# "
                "-----------------------------------------------------------------------" );
            struct {
                skutils::dispatch::queue_id_t id_;
                const skutils::dispatch::priority_t priority_;
                std::atomic_size_t submit_counter_, call_counter_;
                std::atomic_bool bInsideCall_;
                const size_t push_count_at_step_;
                const size_t sleep_milliseconds_;
            } g_arrTestDataByPriority[] = {
                { "-GOD-------", SKUTILS_DISPATCH_PRIORITY_GOD, 0, 0, false, 10, 1 },
                { "-BELOW-GOD-", SKUTILS_DISPATCH_PRIORITY_BELOW_GOD, 0, 0, false, 50, 1 },
                { "-ABSOLUTE--", SKUTILS_DISPATCH_PRIORITY_ABSOLUTE, 0, 0, false, 800, 1 },
                { "-HIGHEST---", SKUTILS_DISPATCH_PRIORITY_HIGHEST, 0, 0, false, 600, 1 },
                { "-HIGH------", SKUTILS_DISPATCH_PRIORITY_HIGH, 0, 0, false, 400, 1 },
                { "-NORMAL----", SKUTILS_DISPATCH_PRIORITY_NORMAL, 0, 0, false, 250, 1 },
                { "-LOW-------", SKUTILS_DISPATCH_PRIORITY_LOW, 0, 0, false, 80, 1 },
                { "-LOWEST----", SKUTILS_DISPATCH_PRIORITY_LOWEST, 0, 0, false, 50, 1 },
                { "-TOO-LOW---", SKUTILS_DISPATCH_PRIORITY_LOWEST * 10, 0, 0, false, 40, 1 },
                { "-EXTRA-LOW-", SKUTILS_DISPATCH_PRIORITY_LOWEST * 100, 0, 0, false, 30, 1 },
                { "-HELL-LOW--", SKUTILS_DISPATCH_PRIORITY_LOWEST * 1000, 0, 0, false, 20, 1 },
            };
            static const size_t cntPriorities =
                sizeof( g_arrTestDataByPriority ) / sizeof( g_arrTestDataByPriority[0] );
            size_t idxPriority;
            {  // block for domain
                skutils::test::test_log_e( thread_prefix_str() + "creating domain instance..." );
                skutils::dispatch::domain_ptr_t pDomain( new skutils::dispatch::domain(
                    nThreadCount ) );  // first parameter threads in pool, if not then then number
                                       // of CPUs
                // skutils::dispatch::domain_ptr_t pDomain( skutils::dispatch::default_domain() );
                //
                std::atomic_bool g_bStopSignalFlag( false ), g_bThreadStoppedFlag( false );
                std::thread( [&]() {
                    skutils::test::test_log_e( thread_prefix_str() + "test thread entered..." );
                    // init queues with priorities
                    for ( idxPriority = 0; idxPriority < cntPriorities; ++idxPriority ) {
                        skutils::dispatch::priority_t pri =
                            g_arrTestDataByPriority[idxPriority].priority_;
                        skutils::dispatch::queue_id_t strQueueID =
                            g_arrTestDataByPriority[idxPriority].id_;
                        skutils::dispatch::queue_ptr_t pQueue(
                            pDomain->queue_get( strQueueID, true ) );
                        pQueue->priority( pri );
                    }  // for( idxPriority = 0; idxPriority < cntPriorities; ++ idxPriority )
                    //
                    // forever loop is just for pushing jobs with tested priorities
                    for ( ; true; ) {
                        for ( idxPriority = 0; idxPriority < cntPriorities; ++idxPriority ) {
                            if ( g_bStopSignalFlag )
                                break;
                            // skutils::dispatch::priority_t pri =
                            //    g_arrTestDataByPriority[idxPriority].priority_;
                            skutils::dispatch::queue_id_t strQueueID =
                                g_arrTestDataByPriority[idxPriority].id_;
                            size_t push_count_at_step =
                                g_arrTestDataByPriority[idxPriority].push_count_at_step_;
                            for ( size_t j = 0; j < push_count_at_step; ++j ) {
                                if ( g_bStopSignalFlag )
                                    break;
                                std::atomic_size_t& nSubmitCounter =
                                    g_arrTestDataByPriority[idxPriority].submit_counter_;
                                ++nSubmitCounter;
                                skutils::dispatch::queue_ptr_t pQueue(
                                    pDomain->queue_get( strQueueID, true ) );
                                pQueue->job_add( [&g_arrTestDataByPriority, idxPriority,
                                                     strQueueID]() {
                                    std::atomic_bool& bInsideCall =
                                        g_arrTestDataByPriority[idxPriority].bInsideCall_;

                                    std::atomic_size_t& nCallCounter =
                                        g_arrTestDataByPriority[idxPriority].call_counter_;
                                    BOOST_REQUIRE( !bool( bInsideCall ) );
                                    // if( bInsideCall ) {
                                    // int xxx = 0;
                                    //}
                                    bInsideCall = true;
                                    ++nCallCounter;
                                    if ( g_bShowDetailedJobLogs )
                                        skutils::test::test_log_e(
                                            thread_prefix_str() + "--- async job in queue " +
                                            strQueueID + ", invocation " +
                                            std::to_string( size_t( nCallCounter ) - 1 ) );
                                    BOOST_REQUIRE( bool( bInsideCall ) );
                                    size_t sleep_milliseconds =
                                        g_arrTestDataByPriority[idxPriority].sleep_milliseconds_;
                                    if ( sleep_milliseconds )
                                        std::this_thread::sleep_for(
                                            std::chrono::milliseconds( sleep_milliseconds ) );
                                    BOOST_REQUIRE( bool( bInsideCall ) );
                                    // if( !bInsideCall ) {
                                    // int xxx = 0;
                                    //}
                                    bInsideCall = false;
                                } );
                            }  // for( size_t j = 0; j < push_count_at_step; ++ j )
                        }  // for( idxPriority = 0; idxPriority < cntPriorities; ++ idxPriority )
                        if ( g_bStopSignalFlag ) {
                            skutils::test::test_log_e(
                                thread_prefix_str() + "test thread got stop signal..." );
                            break;
                        }
                        std::this_thread::sleep_for( std::chrono::milliseconds( 200 ) );
                    }  // for( ; true; )
                    //
                    // signal we are done
                    g_bThreadStoppedFlag = true;
                    skutils::test::test_log_e(
                        thread_prefix_str() + "test thread is about to leave..." );
                } ).detach();
                //
                //
                static const size_t nSleepMilliSeconds = 5000;
                skutils::test::test_log_e( thread_prefix_str() + "will sleep " +
                                           std::to_string( nSleepMilliSeconds ) +
                                           " millisecond(s)..." );
                std::this_thread::sleep_for( std::chrono::milliseconds( nSleepMilliSeconds ) );
                skutils::test::test_log_e( thread_prefix_str() + "done sleeping " +
                                           std::to_string( nSleepMilliSeconds ) +
                                           " millisecond(s)" );
                //
                skutils::test::test_log_e( thread_prefix_str() + "stopping test thread..." );
                g_bStopSignalFlag = true;
                for ( ; !g_bThreadStoppedFlag; )
                    std::this_thread::sleep_for( std::chrono::milliseconds( 20 ) );
                skutils::test::test_log_e(
                    thread_prefix_str() + "test thread was stopped, end of domain life time..." );

                skutils::test::test_log_e( thread_prefix_str() + "shutting down domain..." );
                pDomain->shutdown();
            }  // block for domain
            skutils::test::test_log_e( thread_prefix_str() + "analyzing results..." );
            //
            for ( idxPriority = 0; idxPriority < cntPriorities; ++idxPriority ) {
                skutils::dispatch::queue_id_t strQueueID =
                    g_arrTestDataByPriority[idxPriority].id_;  // skutils::tools::format(
                                                               // "queue_id_%zu", size_t(pri) );
                std::atomic_bool& bInsideCall = g_arrTestDataByPriority[idxPriority].bInsideCall_;
                std::atomic_size_t& nSubmitCounter =
                    g_arrTestDataByPriority[idxPriority].submit_counter_;
                std::atomic_size_t& nCallCounter =
                    g_arrTestDataByPriority[idxPriority].call_counter_;
                BOOST_REQUIRE( nSubmitCounter >= nCallCounter );
                // if( nSubmitCounter < nCallCounter ) {
                // int xxx = 0;
                //}
                double lfPercentHit = ( nSubmitCounter > 0 ) ?
                                          ( ( double( size_t( nCallCounter ) ) /
                                                double( size_t( nSubmitCounter ) ) ) *
                                              100.0 ) :
                                          0.0;
                double lfPercentMiss = ( nSubmitCounter > 0 ) ? ( 100.0 - lfPercentHit ) : 0.0;
                std::string strHit = skutils::tools::format( "%.1lf", lfPercentHit );
                std::string strMiss = skutils::tools::format( "%.1lf", lfPercentMiss );

                skutils::test::test_log_e(
                    thread_prefix_str() + "queue " + strQueueID + " was submitted " +
                    std::to_string( size_t( nSubmitCounter ) ) + " and called " +
                    std::to_string( size_t( nCallCounter ) ) + " time(s)" + ", hit " + strHit +
                    "%" + ", miss " + strMiss + "%" );
                BOOST_REQUIRE( !bool( bInsideCall ) );
                // if( bInsideCall ) {
                // int xxx = 0;
                //}
            }  // for( idxPriority = 0; idxPriority < cntPriorities; ++ idxPriority )
               //				for( idxPriority = 1; idxPriority < cntPriorities; ++ idxPriority )
               //{
               //					//size_t call_counter_prev =
            // g_arrTestDataByPriority[idxPriority-1].call_counter_; 					size_t
            // call_counter_curr = g_arrTestDataByPriority[idxPriority  ].call_counter_;
            // BOOST_REQUIRE( call_counter_curr > 0 );
            //					//BOOST_REQUIRE( call_counter_prev >= call_counter_curr );
            //					//BOOST_REQUIRE( call_counter_prev >= (call_counter_curr/2) ); //
            //---
            //---
            //--- safer condition --- --- ---
            //					//BOOST_REQUIRE( call_counter_prev >= (call_counter_curr/4) ); //
            //---
            //---
            //---
            // even safer condition --- --- --- 				} // for( idxPriority = 1;
            // idxPriority < cntPriorities; ++ idxPriority )
            //
            skutils::test::test_log_e( thread_prefix_str() +
                                       "end of domain priorities test with nThreadCount=" +
                                       std::to_string( nThreadCount ) );
        }  // for( const size_t & nThreadCount : g_arrThreadCounts )
    } );
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

BOOST_AUTO_TEST_CASE( domain_timing_functionality_alive ) {
    skutils::test::test_print_header_name( "SkUtils/dispatch/domain_timing_functionality_alive" );
    skutils::test::with_test_environment( [&]() {
        std::atomic_bool bInsideCallOnce( false );
        std::atomic_size_t nCallCounterOnce( 0 );
        //
        std::atomic_bool bInsideCallPeriodic( false );
        std::atomic_size_t nCallCounterPeriodic( 0 );
        size_t nCallCounterPeriodicExpected( 9 );  // 10 - 1
        //
        std::atomic_bool bInsideCallAsync( false );
        std::atomic_size_t nCallCounterAsync( 0 );
        size_t nCallCounterAsyncExpected( 2 );  // 3 - 1
        //
        skutils::dispatch::queue_id_t sync_queue_id( "sync_queue_id" );
        std::atomic_bool bInsideCallSync( false );
        std::atomic_size_t nCallCounterSync;
        nCallCounterSync = size_t( nCallCounterAsync );
        size_t nCallCounterSyncExpected;
        nCallCounterSyncExpected = size_t( nCallCounterAsyncExpected );
        //
        {  // block for domain
            skutils::test::test_log_e( thread_prefix_str() + "creating domain instance..." );
            skutils::dispatch::domain_ptr_t pDomain( new skutils::dispatch::domain );
            // skutils::dispatch::domain_ptr_t pDomain( skutils::dispatch::default_domain() );
            skutils::dispatch::queue_ptr_t pQueue(
                pDomain->queue_get( skutils::dispatch::get_default_queue_id(), true ) );
            skutils::dispatch::queue_ptr_t pQueueSync( pDomain->queue_get( sync_queue_id, true ) );
            //
            skutils::test::test_log_e( thread_prefix_str() + "adding once job to queue" );
            pQueue->job_add_once(
                [&]() {
                    BOOST_REQUIRE( !bool( bInsideCallOnce ) );
                    bInsideCallOnce = true;
                    ++nCallCounterOnce;
                    skutils::test::test_log_e( thread_prefix_str() + "--- once job, invocation " +
                                               std::to_string( size_t( nCallCounterOnce ) - 1 ) );
                    BOOST_REQUIRE( bool( bInsideCallOnce ) );
                    bInsideCallOnce = false;
                },
                skutils::dispatch::duration_from_seconds( 1 ) );
            //
            skutils::test::test_log_e( thread_prefix_str() + "adding periodic job to queue" );
            pQueue->job_add_periodic(
                [&]() {
                    BOOST_REQUIRE( !bool( bInsideCallPeriodic ) );
                    bInsideCallPeriodic = true;
                    ++nCallCounterPeriodic;
                    skutils::test::test_log_e(
                        thread_prefix_str() + "--- periodic job, invocation " +
                        std::to_string( size_t( nCallCounterPeriodic ) - 1 ) );
                    BOOST_REQUIRE( bool( bInsideCallPeriodic ) );
                    bInsideCallPeriodic = false;
                },
                skutils::dispatch::duration_from_milliseconds( 500 )  // 0.5 seconds
            );
            //
            skutils::test::test_log_e( thread_prefix_str() + "adding async job to queue" );
            pQueue->job_add(
                [&]() {
                    BOOST_REQUIRE( !bool( bInsideCallAsync ) );
                    bInsideCallAsync = true;
                    ++nCallCounterAsync;
                    skutils::test::test_log_e( thread_prefix_str() + "--- async job, invocation " +
                                               std::to_string( size_t( nCallCounterAsync ) - 1 ) );
                    //
                    skutils::test::test_log_e( thread_prefix_str() + "will invoke sync job" );
                    pQueueSync->job_run_sync( [&]() {
                        BOOST_REQUIRE( !bool( bInsideCallSync ) );
                        bInsideCallSync = true;
                        ++nCallCounterSync;
                        skutils::test::test_log_e(
                            thread_prefix_str() + "--- sync job, invocation " +
                            std::to_string( size_t( nCallCounterSync ) - 1 ) );
                        BOOST_REQUIRE( bool( bInsideCallSync ) );
                        bInsideCallSync = false;
                    } );
                    skutils::test::test_log_e( thread_prefix_str() + "did invoked sync job" );
                    //
                    BOOST_REQUIRE( bool( bInsideCallAsync ) );
                    bInsideCallAsync = false;
                },
                skutils::dispatch::duration_from_seconds( 2 ),         // 2 seconds
                skutils::dispatch::duration_from_milliseconds( 1500 )  // 1.5 seconds
            );
            //
            //
            static const size_t nSleepSeconds = 5, nWaitRoundCount = 5;
            for ( size_t nWaitRound = 0; nWaitRound < nWaitRoundCount; ++nWaitRound ) {
                skutils::test::test_log_e(
                    thread_prefix_str() + "waiting for test to complete in round " +
                    std::to_string( nWaitRound + 1 ) + " of " + std::to_string( nWaitRoundCount ) +
                    ", will sleep " + std::to_string( nSleepSeconds ) + " second(s)..." );
                sleep( nSleepSeconds );
                skutils::test::test_log_e( thread_prefix_str() + "done sleeping " +
                                           std::to_string( nSleepSeconds ) +
                                           " second(s), end of domain life time..." );
                if ( size_t( nCallCounterOnce ) >= 1 &&
                     size_t( nCallCounterPeriodic ) >= size_t( nCallCounterPeriodicExpected ) &&
                     size_t( nCallCounterAsync ) >= size_t( nCallCounterAsyncExpected ) &&
                     size_t( nCallCounterSync ) >= size_t( nCallCounterSyncExpected ) )
                    break;
            }  // for( size_t nWaitRound = 0; nWaitRound < nWaitRoundCount; ++ nWaitRound )
            //
            //
            skutils::test::test_log_e( thread_prefix_str() + "shutting down domain..." );
            pDomain->shutdown();
        }  // block for domain
        skutils::test::test_log_e( "once job     expected exactly one call,   was called " +
                                   std::to_string( size_t( nCallCounterOnce ) ) + ", " +
                                   ( ( size_t( nCallCounterOnce ) == 1 ) ? "success" : "fail" ) );
        skutils::test::test_log_e(
            "periodic job expected call(s) at least " +
            std::to_string( size_t( nCallCounterPeriodicExpected ) ) + ", was called " +
            std::to_string( size_t( nCallCounterPeriodic ) ) + ", " +
            ( ( size_t( nCallCounterPeriodic ) >= size_t( nCallCounterPeriodicExpected ) ) ?
                    "success" :
                    "fail" ) );
        skutils::test::test_log_e(
            "async job    expected call(s) at least " +
            std::to_string( size_t( nCallCounterAsyncExpected ) ) + ", was called " +
            std::to_string( size_t( nCallCounterAsync ) ) + ", " +
            ( ( size_t( nCallCounterAsync ) >= size_t( nCallCounterAsyncExpected ) ) ? "success" :
                                                                                       "fail" ) );
        skutils::test::test_log_e(
            "sync job     expected call(s) at least " +
            std::to_string( size_t( nCallCounterSyncExpected ) ) + ", was called " +
            std::to_string( size_t( nCallCounterSync ) ) + ", " +
            ( ( size_t( nCallCounterSync ) >= size_t( nCallCounterSyncExpected ) ) ? "success" :
                                                                                     "fail" ) );
        BOOST_REQUIRE( size_t( nCallCounterOnce ) == 1 );
        BOOST_REQUIRE( size_t( nCallCounterPeriodic ) >= size_t( nCallCounterPeriodicExpected ) );
        BOOST_REQUIRE( size_t( nCallCounterAsync ) >= size_t( nCallCounterAsyncExpected ) );
        BOOST_REQUIRE( size_t( nCallCounterSync ) >= size_t( nCallCounterSyncExpected ) );
        BOOST_REQUIRE( size_t( nCallCounterSync ) == size_t( nCallCounterAsync ) );
        //
        skutils::test::test_log_e(
            thread_prefix_str() + "end of domain timing functionality test" );
    } );
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

BOOST_AUTO_TEST_CASE( simple_api ) {
    skutils::test::test_print_header_name( "SkUtils/dispatch/simple_api" );
    skutils::test::with_test_environment( [&]() {
        std::atomic_bool bInsideCallOnce( false );
        std::atomic_size_t nCallCounterOnce( 0 );
        //
        std::atomic_bool bInsideCallPeriodic( false );
        std::atomic_size_t nCallCounterPeriodic( 0 );
        size_t nCallCounterPeriodicExpected( 9 );  // 10 - 1
        //
        std::atomic_bool bInsideCallAsync( false );
        std::atomic_size_t nCallCounterAsync( 0 );
        size_t nCallCounterAsyncExpected( 2 );  // 3 - 1
        //
        skutils::dispatch::queue_id_t sync_queue_id( "sync_queue_id" );
        std::atomic_bool bInsideCallSync( false );
        std::atomic_size_t nCallCounterSync;
        nCallCounterSync = size_t( nCallCounterAsync );
        size_t nCallCounterSyncExpected;
        nCallCounterSyncExpected = size_t( nCallCounterAsyncExpected );
        //
        // skutils::dispatch::add( sync_queue_id );
        //
        skutils::test::test_log_e( thread_prefix_str() + "adding once job to queue" );
        skutils::dispatch::once(
            skutils::dispatch::get_default_queue_id(),
            [&]() {
                BOOST_REQUIRE( !bool( bInsideCallOnce ) );
                bInsideCallOnce = true;
                ++nCallCounterOnce;
                skutils::test::test_log_e( thread_prefix_str() + "---  once job, invocation " +
                                           std::to_string( size_t( nCallCounterOnce ) - 1 ) );
                BOOST_REQUIRE( bool( bInsideCallOnce ) );
                bInsideCallOnce = false;
            },
            skutils::dispatch::duration_from_seconds( 1 ) );
        //
        skutils::test::test_log_e( thread_prefix_str() + "adding periodic job to queue" );
        skutils::dispatch::repeat(
            skutils::dispatch::get_default_queue_id(),
            [&]() {
                BOOST_REQUIRE( !bool( bInsideCallPeriodic ) );
                bInsideCallPeriodic = true;
                ++nCallCounterPeriodic;
                skutils::test::test_log_e( thread_prefix_str() + "---  periodic job, invocation " +
                                           std::to_string( size_t( nCallCounterPeriodic ) - 1 ) );
                BOOST_REQUIRE( bool( bInsideCallPeriodic ) );
                bInsideCallPeriodic = false;
            },
            skutils::dispatch::duration_from_milliseconds( 500 )  // 0.5 seconds
        );
        //
        skutils::test::test_log_e( thread_prefix_str() + "adding async job to queue" );
        skutils::dispatch::async(
            skutils::dispatch::get_default_queue_id(),
            [&]() {
                BOOST_REQUIRE( !bool( bInsideCallAsync ) );
                bInsideCallAsync = true;
                ++nCallCounterAsync;
                skutils::test::test_log_e( thread_prefix_str() + "--- async job, invocation " +
                                           std::to_string( size_t( nCallCounterAsync ) - 1 ) );
                //
                skutils::test::test_log_e( thread_prefix_str() + "will invoke sync job" );
                skutils::dispatch::sync( sync_queue_id, [&]() {
                    BOOST_REQUIRE( !bool( bInsideCallSync ) );
                    bInsideCallSync = true;
                    ++nCallCounterSync;
                    skutils::test::test_log_e( thread_prefix_str() + "--- sync job, invocation " +
                                               std::to_string( size_t( nCallCounterSync ) - 1 ) );
                    BOOST_REQUIRE( bool( bInsideCallSync ) );
                    bInsideCallSync = false;
                } );
                skutils::test::test_log_e( thread_prefix_str() + "did invoked sync job" );
                //
                BOOST_REQUIRE( bool( bInsideCallAsync ) );
                bInsideCallAsync = false;
            },
            skutils::dispatch::duration_from_seconds( 2 ),         // 2 seconds
            skutils::dispatch::duration_from_milliseconds( 1500 )  // 1.5 seconds
        );
        //
        //
        static const size_t nSleepSeconds = 5, nWaitRoundCount = 5;
        for ( size_t nWaitRound = 0; nWaitRound < nWaitRoundCount; ++nWaitRound ) {
            skutils::test::test_log_e(
                thread_prefix_str() + "waiting for test to complete in round " +
                std::to_string( nWaitRound + 1 ) + " of " + std::to_string( nWaitRoundCount ) +
                ", will sleep " + std::to_string( nSleepSeconds ) + " second(s)..." );
            sleep( nSleepSeconds );
            skutils::test::test_log_e( thread_prefix_str() + "done sleeping " +
                                       std::to_string( nSleepSeconds ) +
                                       " second(s), end of domain life time..." );
            if ( size_t( nCallCounterOnce ) == 1 &&
                 size_t( nCallCounterPeriodic ) >= size_t( nCallCounterPeriodicExpected ) &&
                 size_t( nCallCounterAsync ) >= size_t( nCallCounterAsyncExpected ) &&
                 size_t( nCallCounterSync ) >= size_t( nCallCounterSyncExpected ) )
                break;
        }  // for( size_t nWaitRound = 0; nWaitRound < nWaitRoundCount; ++ nWaitRound )
        //
        //
        skutils::test::test_log_e( thread_prefix_str() + "shutting down default domain..." );
        skutils::dispatch::shutdown();
        //
        //
        skutils::test::test_log_e( "once job     expected exactly one call,   was called " +
                                   std::to_string( size_t( nCallCounterOnce ) ) + ", " +
                                   ( ( size_t( nCallCounterOnce ) == 1 ) ? "success" : "fail" ) );
        skutils::test::test_log_e(
            "periodic job expected call(s) at least " +
            std::to_string( size_t( nCallCounterPeriodicExpected ) ) + ", was called " +
            std::to_string( size_t( nCallCounterPeriodic ) ) + ", " +
            ( ( size_t( nCallCounterPeriodic ) >= size_t( nCallCounterPeriodicExpected ) ) ?
                    "success" :
                    "fail" ) );
        skutils::test::test_log_e(
            "async job    expected call(s) at least " +
            std::to_string( size_t( nCallCounterAsyncExpected ) ) + ", was called " +
            std::to_string( size_t( nCallCounterAsync ) ) + ", " +
            ( ( size_t( nCallCounterAsync ) >= size_t( nCallCounterAsyncExpected ) ) ? "success" :
                                                                                       "fail" ) );
        skutils::test::test_log_e(
            "sync job     expected call(s) at least " +
            std::to_string( size_t( nCallCounterSyncExpected ) ) + ", was called " +
            std::to_string( size_t( nCallCounterSync ) ) + ", " +
            ( ( size_t( nCallCounterSync ) >= size_t( nCallCounterSyncExpected ) ) ? "success" :
                                                                                     "fail" ) );
        BOOST_REQUIRE( size_t( nCallCounterOnce ) == 1 );
        BOOST_REQUIRE( size_t( nCallCounterPeriodic ) >= size_t( nCallCounterPeriodicExpected ) );
        BOOST_REQUIRE( size_t( nCallCounterAsync ) >= size_t( nCallCounterAsyncExpected ) );
        BOOST_REQUIRE( size_t( nCallCounterSync ) >= size_t( nCallCounterSyncExpected ) );
        BOOST_REQUIRE( size_t( nCallCounterSync ) == size_t( nCallCounterAsync ) );
        //
        skutils::test::test_log_e( thread_prefix_str() + "end of simple API test" );
    } );
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

BOOST_AUTO_TEST_CASE( auto_queues ) {
    skutils::test::test_print_header_name( "SkUtils/dispatch/auto_queues" );
    skutils::test::with_test_environment( [&]() {
        std::atomic_size_t nCallCounterAsync( 0 );
        size_t nCallCounterAsyncExpected( 3 );
        //
        skutils::dispatch::queue_id_t sync_queue_id( "sync_queue_id" );
        std::atomic_size_t nCallCounterSync;
        nCallCounterSync = size_t( nCallCounterAsync );
        size_t nCallCounterSyncExpected;
        nCallCounterSyncExpected = size_t( nCallCounterAsyncExpected );
        //
        // skutils::dispatch::add( sync_queue_id );
        //
        skutils::test::test_log_e(
            thread_prefix_str() + "adding async periodical job to auto queue" );
        skutils::dispatch::job_id_t async_job_id0;
        skutils::dispatch::job_id_t async_job_id1;
        skutils::dispatch::job_id_t async_job_id2;
        auto fn = [&]() {
            ++nCallCounterAsync;
            skutils::test::test_log_e( thread_prefix_str() +
                                       "--- async periodical job, invocation " +
                                       std::to_string( size_t( nCallCounterAsync ) - 1 ) );
            //
            skutils::test::test_log_e( thread_prefix_str() + "will invoke " + "sync job" );
            skutils::dispatch::sync( sync_queue_id, [&]() {
                ++nCallCounterSync;
                skutils::test::test_log_e( thread_prefix_str() + "--- sync job" + ", invocation " +
                                           std::to_string( size_t( nCallCounterSync ) - 1 ) );
            } );
            skutils::test::test_log_e( thread_prefix_str() + "did invoked sync job" );
            //
        };
        skutils::dispatch::async( fn,
            skutils::dispatch::duration_from_seconds( 2 ),          // 2 seconds
            skutils::dispatch::duration_from_milliseconds( 1500 ),  // 1.5 seconds
            &async_job_id0 );
        BOOST_REQUIRE( !async_job_id0.empty() );
        skutils::test::test_log_e(
            thread_prefix_str() + "done, did added async periodical job to auto queue, job id is " +
            async_job_id0 );
        skutils::dispatch::async( fn,
            skutils::dispatch::duration_from_seconds( 2 ),          // 2 seconds
            skutils::dispatch::duration_from_milliseconds( 1500 ),  // 1.5 seconds
            &async_job_id1 );
        BOOST_REQUIRE( !async_job_id1.empty() );
        skutils::test::test_log_e(
            thread_prefix_str() + "done, did added async periodical job to auto queue, job id is " +
            async_job_id1 );
        skutils::dispatch::async( fn,
            skutils::dispatch::duration_from_seconds( 2 ),          // 2 seconds
            skutils::dispatch::duration_from_milliseconds( 1500 ),  // 1.5 seconds
            &async_job_id2 );
        BOOST_REQUIRE( !async_job_id2.empty() );
        skutils::test::test_log_e(
            thread_prefix_str() + "done, did added async periodical job to auto queue, job id is " +
            async_job_id2 );
        //
        //
        static const size_t nSleepSeconds = 5, nWaitRoundCount = 5;
        for ( size_t nWaitRound = 0; nWaitRound < nWaitRoundCount; ++nWaitRound ) {
            skutils::test::test_log_e(
                thread_prefix_str() + "waiting for test to complete in round " +
                std::to_string( nWaitRound + 1 ) + " of " + std::to_string( nWaitRoundCount ) +
                ", will sleep " + std::to_string( nSleepSeconds ) + " second(s)..." );
            sleep( nSleepSeconds );
            skutils::test::test_log_e( thread_prefix_str() + "done sleeping " +
                                       std::to_string( nSleepSeconds ) +
                                       " second(s), end of domain life time..." );
            if ( size_t( nCallCounterAsync ) >= size_t( nCallCounterAsyncExpected ) &&
                 size_t( nCallCounterSync ) >= size_t( nCallCounterSyncExpected ) &&
                 size_t( nCallCounterAsync ) >= size_t( nCallCounterAsyncExpected ) &&
                 size_t( nCallCounterSync ) >= size_t( nCallCounterSyncExpected ) &&
                 size_t( nCallCounterSync ) >= size_t( nCallCounterAsync ) )
                break;
        }  // for( size_t nWaitRound = 0; nWaitRound < nWaitRoundCount; ++ nWaitRound )
        //
        //
        skutils::test::test_log_e(
            thread_prefix_str() + "stopping async periodical job " + async_job_id0 + "..." );
        skutils::dispatch::stop( async_job_id0 );
        skutils::test::test_log_e(
            thread_prefix_str() + "stopping async periodical job " + async_job_id1 + "..." );
        skutils::dispatch::stop( async_job_id1 );
        skutils::test::test_log_e(
            thread_prefix_str() + "stopping async periodical job " + async_job_id2 + "..." );
        skutils::dispatch::stop( async_job_id2 );
        //
        skutils::dispatch::set_queue_ids_t setQueueIDs;
        // only "sync_queue_id" should be left
        setQueueIDs = skutils::dispatch::get_all_names();
        BOOST_REQUIRE( setQueueIDs.size() == 1 );
        skutils::dispatch::remove( sync_queue_id );
        // no queues must left
        setQueueIDs = skutils::dispatch::get_all_names();
        BOOST_REQUIRE( setQueueIDs.size() == 0 );
        //
        skutils::test::test_log_e( thread_prefix_str() + "shutting down default domain..." );
        skutils::dispatch::shutdown();
        //
        skutils::test::test_log_e(
            "async job    expected call(s) at least " +
            std::to_string( size_t( nCallCounterAsyncExpected ) ) + ", was called " +
            std::to_string( size_t( nCallCounterAsync ) ) + ", " +
            ( ( size_t( nCallCounterAsync ) >= size_t( nCallCounterAsyncExpected ) ) ? "success" :
                                                                                       "fail" ) );
        skutils::test::test_log_e(
            "sync job     expected call(s) at least " +
            std::to_string( size_t( nCallCounterSyncExpected ) ) + ", was called " +
            std::to_string( size_t( nCallCounterSync ) ) + ", " +
            ( ( size_t( nCallCounterSync ) >= size_t( nCallCounterSyncExpected ) ) ? "success" :
                                                                                     "fail" ) );
        BOOST_REQUIRE( size_t( nCallCounterAsync ) >= size_t( nCallCounterAsyncExpected ) );
        BOOST_REQUIRE( size_t( nCallCounterSync ) >= size_t( nCallCounterSyncExpected ) );
        BOOST_REQUIRE( size_t( nCallCounterSync ) >= size_t( nCallCounterAsync ) );
        //
        skutils::test::test_log_e( thread_prefix_str() + "end of auto_queues test" );
    } );
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

BOOST_AUTO_TEST_CASE( cross_jobs ) {
    skutils::test::test_print_header_name( "SkUtils/dispatch/cross_jobs" );
    skutils::test::with_test_environment( [&]() {
        const size_t nQueueCount = 5;
        std::vector< bool > vecInside;
        std::vector< size_t > vecCallCount;
        size_t i;
        for ( i = 0; i < nQueueCount; ++i ) {
            vecInside.push_back( false );
            vecCallCount.push_back( 0 );
            skutils::dispatch::queue_id_t id_queue_current =
                skutils::tools::format( "queue_%zu", i );
            skutils::dispatch::add( id_queue_current );
        }
        for ( i = 0; i < nQueueCount; ++i ) {
            size_t j = ( i + 1 ) % nQueueCount;
            skutils::dispatch::queue_id_t id_queue_current =
                skutils::tools::format( "queue_%zu", i );
            skutils::dispatch::queue_id_t id_queue_next = skutils::tools::format( "queue_%zu", j );
            if ( g_bShowDetailedJobLogs )
                skutils::test::test_log_e( thread_prefix_str() + "...will add async job to queue " +
                                           id_queue_current + "..." );
            skutils::dispatch::async( id_queue_current, [&vecInside, &vecCallCount, i, j,
                                                            id_queue_current, id_queue_next]() {
                BOOST_REQUIRE( !bool( vecInside[i] ) );
                vecInside[i] = true;
                BOOST_REQUIRE( bool( vecInside[i] ) );
                vecCallCount[i]++;
                if ( i != 0 ) {  // condition to avoid chained lock
                    skutils::test::test_log_e( thread_prefix_str() + "--> worker " +
                                               id_queue_current + " will invoke " + id_queue_next );
                    skutils::dispatch::sync( id_queue_next,
                        [&vecInside, &vecCallCount, i, j, id_queue_current, id_queue_next]() {
                            BOOST_REQUIRE( !bool( vecInside[j] ) );
                            vecInside[j] = true;
                            BOOST_REQUIRE( bool( vecInside[j] ) );
                            vecCallCount[j]++;
                            skutils::test::test_log_e( thread_prefix_str() + "<-- worker " +
                                                       id_queue_next + " invoked from " +
                                                       id_queue_current );
                            BOOST_REQUIRE( bool( vecInside[j] ) );
                            vecInside[j] = false;
                            BOOST_REQUIRE( !bool( vecInside[j] ) );
                        } );
                } else {
                    skutils::test::test_log_e( thread_prefix_str() + "--> worker " +
                                               id_queue_current + " will invoke " + id_queue_next +
                                               "(emulation)" );
                    skutils::test::test_log_e( thread_prefix_str() + "<-- worker " + id_queue_next +
                                               " invoked from " + id_queue_current +
                                               "(emulation)" );
                    vecCallCount[j]++;  // invocation emilation
                }
                BOOST_REQUIRE( bool( vecInside[i] ) );
                vecInside[i] = false;
                BOOST_REQUIRE( !bool( vecInside[i] ) );
            } );
        }
        //
        //
        bool isEverythingOKay = true;  // assume good thing
        static const size_t nSleepSeconds = 5, nWaitRoundCount = 5;
        for ( size_t nWaitRound = 0; nWaitRound < nWaitRoundCount; ++nWaitRound ) {
            skutils::test::test_log_e(
                thread_prefix_str() + "waiting for test to complete in round " +
                std::to_string( nWaitRound + 1 ) + " of " + std::to_string( nWaitRoundCount ) +
                ", will sleep " + std::to_string( nSleepSeconds ) + " second(s)..." );
            sleep( nSleepSeconds );
            skutils::test::test_log_e( thread_prefix_str() + "done sleeping " +
                                       std::to_string( nSleepSeconds ) +
                                       " second(s), end of domain life time..." );
            //
            //
            // pre-liminary attempt to find out everything is OKay
            skutils::test::test_log_e(
                thread_prefix_str() + "performing preliminary state test..." );
            isEverythingOKay = true;  // assume good thing
            for ( i = 0; i < nQueueCount; ++i ) {
                bool bInside = bool( vecInside[i] );
                skutils::test::test_log_e( thread_prefix_str() + "queue " + std::to_string( i ) +
                                           " is " +
                                           ( ( !bInside ) ? "OKay" : "STILL WORKING - FAIL" ) );
                if ( bInside ) {
                    isEverythingOKay = false;
                    break;
                }
                skutils::dispatch::queue_id_t id_queue_current =
                    skutils::tools::format( "queue_%zu", i );
                skutils::dispatch::queue_ptr_t pQueue =
                    skutils::dispatch::get( id_queue_current, false );
                size_t nQueueJobCount = pQueue->async_job_count();
                // BOOST_REQUIRE( nQueueJobCount == 0 );
                skutils::test::test_log_e(
                    thread_prefix_str() + "worker " + id_queue_current + " has " +
                    std::to_string( nQueueJobCount ) + " job(s) unfinished " +
                    ( ( nQueueJobCount == 0 ) ? "OKay" : "FAIL, MUST BE ZERO" ) );
            }
            skutils::test::test_log_e( thread_prefix_str() + "done preliminary state test - " +
                                       ( isEverythingOKay ? "PASSED" : "FAILED" ) );
            if ( isEverythingOKay )
                break;
        }  // for( size_t nWaitRound = 0; nWaitRound < nWaitRoundCount; ++ nWaitRound )
        //
        //
        if ( !isEverythingOKay ) {
            static const size_t nSleepSecondsExtraAttempt = 10;
            skutils::test::test_log_e( thread_prefix_str() + "will sleep additional " +
                                       std::to_string( nSleepSecondsExtraAttempt ) +
                                       " second(s)..." );
            sleep( nSleepSecondsExtraAttempt );
            skutils::test::test_log_e( thread_prefix_str() + "done sleeping additional " +
                                       std::to_string( nSleepSecondsExtraAttempt ) +
                                       " second(s), end of domain life time..." );
        }
        //
        // final test
        skutils::test::test_log_e( thread_prefix_str() + "performing final state test..." );
        for ( i = 0; i < nQueueCount; ++i ) {
            bool bInside = bool( vecInside[i] );
            skutils::test::test_log_e( thread_prefix_str() + "queue " + std::to_string( i ) +
                                       " is " +
                                       ( ( !bInside ) ? "OKay" : "STILL WORKING - FAIL" ) );
            BOOST_REQUIRE( !bInside );
            skutils::dispatch::queue_id_t id_queue_current =
                skutils::tools::format( "queue_%zu", i );
            skutils::dispatch::queue_ptr_t pQueue =
                skutils::dispatch::get( id_queue_current, false );
            size_t nQueueJobCount = pQueue->async_job_count();
            // BOOST_REQUIRE( nQueueJobCount == 0 );
            skutils::test::test_log_e(
                thread_prefix_str() + "worker " + id_queue_current + " has " +
                std::to_string( nQueueJobCount ) + " job(s) unfinished " +
                ( ( nQueueJobCount == 0 ) ? "OKay" : "FAIL, MUST BE ZERO" ) );
        }
        skutils::test::test_log_e( thread_prefix_str() + "done final state test - PASSED" );
        //
        skutils::test::test_log_e( thread_prefix_str() + "shutting down default domain..." );
        skutils::dispatch::shutdown();
        //
        for ( i = 0; i < nQueueCount; ++i ) {
            BOOST_REQUIRE( !bool( vecInside[i] ) );
            skutils::dispatch::queue_id_t id_queue_current =
                skutils::tools::format( "queue_%zu", i );
            skutils::test::test_log_e( thread_prefix_str() + "worker " + id_queue_current +
                                       " was invoked " + std::to_string( vecCallCount[i] ) +
                                       " time(s)" );
        }
        size_t nCallCountFirst = vecCallCount.front();
        BOOST_REQUIRE( nCallCountFirst > 0 );
        BOOST_REQUIRE( ( nCallCountFirst & 1 ) == 0 );
        for ( i = 1; i < nQueueCount; ++i ) {
            BOOST_REQUIRE( vecCallCount[i] == nCallCountFirst );
        }
        //
        skutils::test::test_log_e( thread_prefix_str() + "end of cross_jobs test" );
    } );
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

BOOST_AUTO_TEST_CASE( enqueue_while_busy ) {
    skutils::test::test_print_header_name( "SkUtils/dispatch/enqueue_while_busy" );
    skutils::test::with_test_environment( [&]() {
        skutils::dispatch::queue_id_t id_my_queue( "some queue" );
        typedef std::vector< std::string > string_vector_t;
        string_vector_t log_sequence;
        typedef std::mutex mutex_type;
        typedef std::lock_guard< mutex_type > lock_type;
        mutex_type mtx;
        auto fnLog = [&]( const char* s ) -> void {
            lock_type lock( mtx );
            log_sequence.push_back( s );
            skutils::test::test_log_e( thread_prefix_str() + "--- " + s );
        };
        auto fnLogSequenceSize = [&]() -> size_t {
            lock_type lock( mtx );
            size_t n = log_sequence.size();
            return n;
        };
        const char g_strLogText_LengthyWork_begin[] = "lengthy work begin";
        const char g_strLogText_LengthyWork_end[] = "lengthy work end";
        const char g_strLogText_SyncWork_begin[] = "sync work begin";
        const char g_strLogText_SyncWork_end[] = "sync work end";
        const char g_strLogText_ShortWork_0_begin[] = "short work 0 begin";
        const char g_strLogText_ShortWork_0_end[] = "short work 0 end";
        const char g_strLogText_ShortWork_1_begin[] = "short work 1 begin";
        const char g_strLogText_ShortWork_1_end[] = "short work 1 end";
        const char g_strLogText_ShortWork_2_begin[] = "short work 2 begin";
        const char g_strLogText_ShortWork_2_end[] = "short work 2 end";
        std::atomic_bool bInside_LengthyWork( false );
        std::atomic_bool bInside_ShortWork_0( false );
        std::atomic_bool bInside_ShortWork_1( false );
        std::atomic_bool bInside_ShortWork_2( false );
        std::atomic_bool bInside_SyncWork( false );
        //
        skutils::dispatch::async( id_my_queue, [&]() {
            BOOST_REQUIRE( !bool( bInside_LengthyWork ) );
            BOOST_REQUIRE( !bool( bInside_ShortWork_0 ) );
            BOOST_REQUIRE( !bool( bInside_ShortWork_1 ) );
            BOOST_REQUIRE( !bool( bInside_ShortWork_2 ) );
            BOOST_REQUIRE( !bool( bInside_SyncWork ) );
            bInside_LengthyWork = true;
            BOOST_REQUIRE( bool( bInside_LengthyWork ) );
            BOOST_REQUIRE( !bool( bInside_ShortWork_0 ) );
            BOOST_REQUIRE( !bool( bInside_ShortWork_1 ) );
            BOOST_REQUIRE( !bool( bInside_ShortWork_2 ) );
            BOOST_REQUIRE( !bool( bInside_SyncWork ) );
            fnLog( g_strLogText_LengthyWork_begin );
            std::this_thread::sleep_for( std::chrono::seconds( 2 ) );
            fnLog( g_strLogText_LengthyWork_end );
            bInside_LengthyWork = false;
            BOOST_REQUIRE( !bool( bInside_LengthyWork ) );
            BOOST_REQUIRE( !bool( bInside_ShortWork_0 ) );
            BOOST_REQUIRE( !bool( bInside_ShortWork_1 ) );
            BOOST_REQUIRE( !bool( bInside_ShortWork_2 ) );
            BOOST_REQUIRE( !bool( bInside_SyncWork ) );
        } );
        skutils::dispatch::async( id_my_queue, [&]() {
            BOOST_REQUIRE( !bool( bInside_LengthyWork ) );
            BOOST_REQUIRE( !bool( bInside_ShortWork_0 ) );
            BOOST_REQUIRE( !bool( bInside_ShortWork_1 ) );
            BOOST_REQUIRE( !bool( bInside_ShortWork_2 ) );
            BOOST_REQUIRE( !bool( bInside_SyncWork ) );
            bInside_ShortWork_0 = true;
            BOOST_REQUIRE( !bool( bInside_LengthyWork ) );
            BOOST_REQUIRE( bool( bInside_ShortWork_0 ) );
            BOOST_REQUIRE( !bool( bInside_ShortWork_1 ) );
            BOOST_REQUIRE( !bool( bInside_ShortWork_2 ) );
            BOOST_REQUIRE( !bool( bInside_SyncWork ) );
            BOOST_REQUIRE( bool( bInside_ShortWork_0 ) );
            fnLog( g_strLogText_ShortWork_0_begin );
            std::this_thread::sleep_for( std::chrono::milliseconds( 10 ) );
            fnLog( g_strLogText_ShortWork_0_end );
            BOOST_REQUIRE( !bool( bInside_LengthyWork ) );
            BOOST_REQUIRE( bool( bInside_ShortWork_0 ) );
            BOOST_REQUIRE( !bool( bInside_ShortWork_1 ) );
            BOOST_REQUIRE( !bool( bInside_ShortWork_2 ) );
            BOOST_REQUIRE( !bool( bInside_SyncWork ) );
            BOOST_REQUIRE( bool( bInside_ShortWork_0 ) );
            bInside_ShortWork_0 = false;
            BOOST_REQUIRE( !bool( bInside_LengthyWork ) );
            BOOST_REQUIRE( !bool( bInside_ShortWork_0 ) );
            BOOST_REQUIRE( !bool( bInside_ShortWork_1 ) );
            BOOST_REQUIRE( !bool( bInside_ShortWork_2 ) );
            BOOST_REQUIRE( !bool( bInside_SyncWork ) );
        } );
        skutils::dispatch::async( id_my_queue, [&]() {
            BOOST_REQUIRE( !bool( bInside_LengthyWork ) );
            BOOST_REQUIRE( !bool( bInside_ShortWork_0 ) );
            BOOST_REQUIRE( !bool( bInside_ShortWork_1 ) );
            BOOST_REQUIRE( !bool( bInside_ShortWork_2 ) );
            BOOST_REQUIRE( !bool( bInside_SyncWork ) );
            bInside_ShortWork_1 = true;
            BOOST_REQUIRE( !bool( bInside_LengthyWork ) );
            BOOST_REQUIRE( !bool( bInside_ShortWork_0 ) );
            BOOST_REQUIRE( bool( bInside_ShortWork_1 ) );
            BOOST_REQUIRE( !bool( bInside_ShortWork_2 ) );
            BOOST_REQUIRE( !bool( bInside_SyncWork ) );
            fnLog( g_strLogText_ShortWork_1_begin );
            std::this_thread::sleep_for( std::chrono::milliseconds( 10 ) );
            fnLog( g_strLogText_ShortWork_1_end );
            BOOST_REQUIRE( !bool( bInside_LengthyWork ) );
            BOOST_REQUIRE( !bool( bInside_ShortWork_0 ) );
            BOOST_REQUIRE( bool( bInside_ShortWork_1 ) );
            BOOST_REQUIRE( !bool( bInside_ShortWork_2 ) );
            BOOST_REQUIRE( !bool( bInside_SyncWork ) );
            bInside_ShortWork_1 = false;
            BOOST_REQUIRE( !bool( bInside_LengthyWork ) );
            BOOST_REQUIRE( !bool( bInside_ShortWork_0 ) );
            BOOST_REQUIRE( !bool( bInside_ShortWork_1 ) );
            BOOST_REQUIRE( !bool( bInside_ShortWork_2 ) );
            BOOST_REQUIRE( !bool( bInside_SyncWork ) );
        } );
        skutils::dispatch::async( id_my_queue, [&]() {
            BOOST_REQUIRE( !bool( bInside_LengthyWork ) );
            BOOST_REQUIRE( !bool( bInside_ShortWork_0 ) );
            BOOST_REQUIRE( !bool( bInside_ShortWork_1 ) );
            BOOST_REQUIRE( !bool( bInside_ShortWork_2 ) );
            BOOST_REQUIRE( !bool( bInside_SyncWork ) );
            bInside_ShortWork_2 = true;
            BOOST_REQUIRE( !bool( bInside_LengthyWork ) );
            BOOST_REQUIRE( !bool( bInside_ShortWork_0 ) );
            BOOST_REQUIRE( !bool( bInside_ShortWork_1 ) );
            BOOST_REQUIRE( bool( bInside_ShortWork_2 ) );
            BOOST_REQUIRE( !bool( bInside_SyncWork ) );
            fnLog( g_strLogText_ShortWork_2_begin );
            std::this_thread::sleep_for( std::chrono::milliseconds( 10 ) );
            fnLog( g_strLogText_ShortWork_2_end );
            BOOST_REQUIRE( !bool( bInside_LengthyWork ) );
            BOOST_REQUIRE( !bool( bInside_ShortWork_0 ) );
            BOOST_REQUIRE( !bool( bInside_ShortWork_1 ) );
            BOOST_REQUIRE( bool( bInside_ShortWork_2 ) );
            BOOST_REQUIRE( !bool( bInside_SyncWork ) );
            bInside_ShortWork_2 = false;
            BOOST_REQUIRE( !bool( bInside_LengthyWork ) );
            BOOST_REQUIRE( !bool( bInside_ShortWork_0 ) );
            BOOST_REQUIRE( !bool( bInside_ShortWork_1 ) );
            BOOST_REQUIRE( !bool( bInside_ShortWork_2 ) );
            BOOST_REQUIRE( !bool( bInside_SyncWork ) );
        } );
        skutils::test::test_log_e( thread_prefix_str() + "awaiting 500 milliseconds" );
        std::this_thread::sleep_for( std::chrono::milliseconds( 500 ) );
        skutils::test::test_log_e(
            thread_prefix_str() + "done, finished awaiting 500 milliseconds" );
        skutils::dispatch::sync( id_my_queue, [&]() {
            BOOST_REQUIRE( !bool( bInside_LengthyWork ) );
            BOOST_REQUIRE( !bool( bInside_ShortWork_0 ) );
            BOOST_REQUIRE( !bool( bInside_ShortWork_1 ) );
            BOOST_REQUIRE( !bool( bInside_ShortWork_2 ) );
            BOOST_REQUIRE( !bool( bInside_SyncWork ) );
            bInside_SyncWork = true;
            BOOST_REQUIRE( !bool( bInside_LengthyWork ) );
            BOOST_REQUIRE( !bool( bInside_ShortWork_0 ) );
            BOOST_REQUIRE( !bool( bInside_ShortWork_1 ) );
            BOOST_REQUIRE( !bool( bInside_ShortWork_2 ) );
            BOOST_REQUIRE( bool( bInside_SyncWork ) );
            fnLog( g_strLogText_SyncWork_begin );
            std::this_thread::sleep_for( std::chrono::milliseconds( 100 ) );
            fnLog( g_strLogText_SyncWork_end );
            BOOST_REQUIRE( !bool( bInside_LengthyWork ) );
            BOOST_REQUIRE( !bool( bInside_ShortWork_0 ) );
            BOOST_REQUIRE( !bool( bInside_ShortWork_1 ) );
            BOOST_REQUIRE( !bool( bInside_ShortWork_2 ) );
            BOOST_REQUIRE( bool( bInside_SyncWork ) );
            bInside_SyncWork = false;
            BOOST_REQUIRE( !bool( bInside_LengthyWork ) );
            BOOST_REQUIRE( !bool( bInside_ShortWork_0 ) );
            BOOST_REQUIRE( !bool( bInside_ShortWork_1 ) );
            BOOST_REQUIRE( !bool( bInside_ShortWork_2 ) );
            BOOST_REQUIRE( !bool( bInside_SyncWork ) );
        } );
        //
        //
        static const size_t nSleepSeconds = 5, nWaitRoundCount = 5;
        for ( size_t nWaitRound = 0; nWaitRound < nWaitRoundCount; ++nWaitRound ) {
            skutils::test::test_log_e(
                thread_prefix_str() + "waiting for test to complete in round " +
                std::to_string( nWaitRound + 1 ) + " of " + std::to_string( nWaitRoundCount ) +
                ", will sleep " + std::to_string( nSleepSeconds ) + " second(s)..." );
            sleep( nSleepSeconds );
            skutils::test::test_log_e( thread_prefix_str() + "done sleeping " +
                                       std::to_string( nSleepSeconds ) +
                                       " second(s), end of domain life time..." );
            size_t n = fnLogSequenceSize();
            if ( n >= 10 )
                break;
        }  // for( size_t nWaitRound = 0; nWaitRound < nWaitRoundCount; ++ nWaitRound )
        //
        //
        skutils::test::test_log_e( thread_prefix_str() + "shutting down default domain..." );
        skutils::dispatch::shutdown();
        //
        //
        skutils::test::test_log_e( thread_prefix_str() + "analyzing expected results..." );
        BOOST_REQUIRE( log_sequence.size() >= 10 );
        BOOST_REQUIRE( log_sequence[0] == g_strLogText_LengthyWork_begin );
        BOOST_REQUIRE( log_sequence[1] == g_strLogText_LengthyWork_end );
        BOOST_REQUIRE( log_sequence[2] == g_strLogText_SyncWork_begin );
        BOOST_REQUIRE( log_sequence[3] == g_strLogText_SyncWork_end );
        BOOST_REQUIRE( log_sequence[4] == g_strLogText_ShortWork_0_begin );
        BOOST_REQUIRE( log_sequence[5] == g_strLogText_ShortWork_0_end );
        BOOST_REQUIRE( log_sequence[6] == g_strLogText_ShortWork_1_begin );
        BOOST_REQUIRE( log_sequence[7] == g_strLogText_ShortWork_1_end );
        BOOST_REQUIRE( log_sequence[8] == g_strLogText_ShortWork_2_begin );
        BOOST_REQUIRE( log_sequence[9] == g_strLogText_ShortWork_2_end );
        //
        skutils::test::test_log_e( thread_prefix_str() + "end of enqueue_while_busy test" );
    } );
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// temporary disable the test
// the functionality it tests is not used
// BOOST_AUTO_TEST_CASE( balance_equality ) {
//    skutils::test::test_print_header_name( "SkUtils/dispatch/balance_equality" );
//    skutils::test::with_test_environment( [&]() {
//        typedef std::map< skutils::dispatch::queue_id_t, size_t > map_call_counts_t;
//        map_call_counts_t mapCallCounts, mapJobsLeft;
//        typedef std::mutex mutex_type;
//        typedef std::lock_guard< mutex_type > lock_type;
//        mutex_type mtx;
//        auto fnLogCall = [&]( const skutils::dispatch::queue_id_t& id, size_t& nCallsOut ) -> void
//        {
//            lock_type lock( mtx );
//            map_call_counts_t::iterator itFind = mapCallCounts.find( id ), itEnd =
//                                                                               mapCallCounts.end();
//            if ( itFind != itEnd ) {
//                size_t cntCalls = itFind->second;
//                ++cntCalls;
//                itFind->second = cntCalls;
//                nCallsOut = cntCalls;
//            } else {
//                mapCallCounts[id] = 1;
//                nCallsOut = 1;
//            }
//        };
//        //
//        static const size_t cntThreads = 16;
//        skutils::test::test_log_e( thread_prefix_str() + cc::debug( "will use " ) +
//                                   cc::size10( cntThreads ) + cc::debug( " threads(s)..." ) );
//        skutils::dispatch::default_domain( cntThreads );  // use 16 threads in default domain
//        static const size_t cntQueues = 500, cntJobs = 200, nSleepMillisecondsInJob = 0;
//        const size_t cntExpectedCalls = cntQueues * cntJobs;
//        skutils::test::test_log_e( thread_prefix_str() + cc::debug( "will run " ) +
//                                   cc::size10( cntQueues ) + cc::debug( " queue(s) with " ) +
//                                   cc::size10( cntJobs ) + cc::debug( " job(s) in each..." ) );
//        skutils::test::test_log_e( thread_prefix_str() +
//                                   cc::debug( "... so max expected call count is " ) +
//                                   cc::size10( cntExpectedCalls ) );
//        skutils::test::test_log_e( thread_prefix_str() + cc::debug( "overloading queues... " ) );
//        size_t i, j;
//        for ( j = 0; j < cntJobs; ++j ) {
//            for ( i = 0; i < cntQueues; ++i ) {
//                skutils::dispatch::queue_id_t id_my_queue =
//                    skutils::tools::format( "queue_%zu", i );
//                skutils::dispatch::async( id_my_queue, [id_my_queue, &fnLogCall]() {
//                    size_t nCalls = 0;
//                    fnLogCall( id_my_queue, nCalls );
//                    BOOST_REQUIRE( nCalls > 0 );
//                    //							if( g_bShowDetailedJobLogs )
//                    //								skutils::test::test_log_e(
//                    thread_prefix_str()
//                    +
//                    // cc::debug("--- async job in queue ") + cc::info(id_my_queue) + cc::debug(",
//                    // invocation ") +
//                    // cc::size10(size_t(nCalls)-1) );
//                    std::this_thread::sleep_for(
//                        std::chrono::milliseconds( nSleepMillisecondsInJob ) );
//                } );
//            }  // for...
//        }      // for...
//        skutils::test::test_log_e( thread_prefix_str() + cc::debug( "done overloading queues, " )
//        +
//                                   cc::size10( cntExpectedCalls ) + cc::debug( " jobs(s) added" )
//                                   );
//        static const size_t nSleepSeconds = 5;
//        skutils::test::test_log_e( thread_prefix_str() + cc::warn( "will sleep " ) +
//                                   cc::size10( nSleepSeconds ) + cc::warn( " second(s)..." ) );
//        sleep( nSleepSeconds );
//        skutils::test::test_log_e( thread_prefix_str() + cc::warn( "done sleeping " ) +
//                                   cc::size10( nSleepSeconds ) +
//                                   cc::warn( " second(s), end of domain life time..." ) );
//        //
//        for ( const auto& entry : mapCallCounts ) {
//            skutils::dispatch::queue_ptr_t pQueue = skutils::dispatch::get( entry.first, false );
//            size_t jobCountInQueue = pQueue->async_job_count();
//            // if( jobCountInQueue > 0 ) {
//            //	int xxx = 0;
//            //}
//            mapJobsLeft[entry.first] = jobCountInQueue;
//        }
//        //
//        skutils::test::test_log_e(
//            thread_prefix_str() + cc::warn( "shutting down default domain..." ) );
//        skutils::dispatch::shutdown();
//        //
//        skutils::test::test_log_e(
//            thread_prefix_str() + cc::warn( "analyzing expected results..." ) );
//        if ( g_bShowDetailedJobLogs ) {
//            for ( const auto& entry : mapCallCounts ) {
//                std::string s = thread_prefix_str() + cc::debug( "queue " ) +
//                                cc::info( entry.first ) + cc::debug( " did performed " ) +
//                                cc::size10( entry.second ) + cc::debug( " call(s)" );
//                size_t jobCountInQueue = mapJobsLeft[entry.first];
//                if ( jobCountInQueue > 0 )
//                    s += cc::debug( ", " ) + cc::size10( jobCountInQueue ) +
//                         cc::debug( " job(s) left" );
//                skutils::test::test_log_e( s );
//            }
//        }
//        i = 0;
//        size_t nMin = 0, nMax = 0, nCallsSummary = 0;
//        for ( const auto& entry : mapCallCounts ) {
//            nCallsSummary += entry.second;
//            if ( i == 0 )
//                nMin = nMax = entry.second;
//            else {
//                if ( nMin > entry.second )
//                    nMin = entry.second;
//                if ( nMax < entry.second )
//                    nMax = entry.second;
//            }
//            ++i;
//        }
//        BOOST_REQUIRE( nMax > 0 );
//        double lfMin = ( double( nMin ) / double( nMax ) ) * 100.0;
//        skutils::test::test_log_e( thread_prefix_str() + cc::debug( "got " ) + cc::size10( nMin )
//        +
//                                   cc::debug( " min call(s) and " ) + cc::size10( nMax ) +
//                                   cc::debug( " max calls" ) );
//        skutils::test::test_log_e( thread_prefix_str() + cc::debug( "got min as " ) +
//                                   cc::note( skutils::tools::format( "%.1lf", lfMin ) ) +
//                                   cc::debug( "%, if assuming max as " ) + cc::size10( 100 ) +
//                                   cc::debug( "%" ) );
//        BOOST_REQUIRE( lfMin >= 80.0 );
//        //
//        skutils::test::test_log_e(
//            thread_prefix_str() + cc::debug( "got " ) + cc::size10( nCallsSummary ) +
//            cc::debug( " call(s) done, max expected calls is " ) + cc::size10( cntExpectedCalls )
//            );
//        double lfCallsPercent = ( double( nCallsSummary ) / double( cntExpectedCalls ) ) * 100.0;
//        skutils::test::test_log_e( thread_prefix_str() + cc::debug( "got real calls as " ) +
//                                   cc::note( skutils::tools::format( "%.1lf", lfCallsPercent ) ) +
//                                   cc::debug( "%, if assuming max calls as " ) + cc::size10( 100 )
//                                   + cc::debug( "%" ) );
//        //
//        //
//        skutils::test::test_log_e(
//            thread_prefix_str() + cc::info( "end of balance_equality test" ) );
//    } );
//}

BOOST_AUTO_TEST_SUITE_END()
BOOST_AUTO_TEST_SUITE_END()
