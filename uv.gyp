{
  'target_defaults': {
    'conditions': [
      ['OS != "win"', {
        'defines': [
          '_LARGEFILE_SOURCE',
          '_FILE_OFFSET_BITS=64',
          '_GNU_SOURCE',
          'EIO_STACKSIZE=262144'
        ],
        'conditions': [
          ['OS=="solaris"', {
            'cflags': [ '-pthreads' ],
          }, {
            'cflags': [ '-pthread' ],
          }],
        ],
      }],
    ],
  },

  'targets': [
    {
      'target_name': 'uv',
      'type': '<(library)',
      'include_dirs': [
        'include',
        'include/uv-private',
        'src/',
        'src/UDT4/src/',
      ],
      'direct_dependent_settings': {
        'include_dirs': [ 'include' ],
      },
      'defines': [
        'HAVE_CONFIG_H'
      ],
      'sources': [
        'common.gypi',
        'include/ares.h',
        'include/ares_version.h',
        'include/uv.h',
        'include/uv-private/ngx-queue.h',
        'include/uv-private/tree.h',
        'src/UDT4/src/udtc.h',
        'src/cares.c',
        'src/fs-poll.c',
        'src/uv-common.c',
        'src/uv-common.h',
        'src/ares/ares_cancel.c',
        'src/ares/ares__close_sockets.c',
        'src/ares/ares_data.c',
        'src/ares/ares_data.h',
        'src/ares/ares_destroy.c',
        'src/ares/ares_dns.h',
        'src/ares/ares_expand_name.c',
        'src/ares/ares_expand_string.c',
        'src/ares/ares_fds.c',
        'src/ares/ares_free_hostent.c',
        'src/ares/ares_free_string.c',
        'src/ares/ares_getenv.h',
        'src/ares/ares_gethostbyaddr.c',
        'src/ares/ares_gethostbyname.c',
        'src/ares/ares__get_hostent.c',
        'src/ares/ares_getnameinfo.c',
        'src/ares/ares_getopt.c',
        'src/ares/ares_getopt.h',
        'src/ares/ares_getsock.c',
        'src/ares/ares_init.c',
        'src/ares/ares_ipv6.h',
        'src/ares/ares_library_init.c',
        'src/ares/ares_library_init.h',
        'src/ares/ares_llist.c',
        'src/ares/ares_llist.h',
        'src/ares/ares_mkquery.c',
        'src/ares/ares_nowarn.c',
        'src/ares/ares_nowarn.h',
        'src/ares/ares_options.c',
        'src/ares/ares_parse_aaaa_reply.c',
        'src/ares/ares_parse_a_reply.c',
        'src/ares/ares_parse_mx_reply.c',
        'src/ares/ares_parse_ns_reply.c',
        'src/ares/ares_parse_ptr_reply.c',
        'src/ares/ares_parse_srv_reply.c',
        'src/ares/ares_parse_txt_reply.c',
        'src/ares/ares_platform.h',
        'src/ares/ares_private.h',
        'src/ares/ares_process.c',
        'src/ares/ares_query.c',
        'src/ares/ares__read_line.c',
        'src/ares/ares_rules.h',
        'src/ares/ares_search.c',
        'src/ares/ares_send.c',
        'src/ares/ares_setup.h',
        'src/ares/ares_strcasecmp.c',
        'src/ares/ares_strcasecmp.h',
        'src/ares/ares_strdup.c',
        'src/ares/ares_strdup.h',
        'src/ares/ares_strerror.c',
        'src/ares/ares_timeout.c',
        'src/ares/ares__timeval.c',
        'src/ares/ares_version.c',
        'src/ares/ares_writev.c',
        'src/ares/ares_writev.h',
        'src/ares/bitncmp.c',
        'src/ares/bitncmp.h',
        'src/ares/inet_net_pton.c',
        'src/ares/inet_net_pton.h',
        'src/ares/inet_ntop.c',
        'src/ares/inet_ntop.h',
        'src/ares/nameser.h',
        'src/ares/setup_once.h',
        'src/ares/windows_port.c',
        'src/UDT4/src/api.cpp',
        'src/UDT4/src/buffer.cpp',
        'src/UDT4/src/cache.cpp',
        'src/UDT4/src/ccc.cpp',
        'src/UDT4/src/channel.cpp',
        'src/UDT4/src/common.cpp',
        'src/UDT4/src/udt_core.cpp',
        'src/UDT4/src/epoll.cpp',
        'src/UDT4/src/list.cpp',
        'src/UDT4/src/md5.cpp',
        'src/UDT4/src/packet.cpp',
        'src/UDT4/src/queue.cpp',
        'src/UDT4/src/udtc.cpp',
        'src/UDT4/src/window.cpp',
      ],
      'conditions': [
        [ 'OS=="win"', {
          'include_dirs': [
            'src/ares/config_win32'
          ],
          'defines': [
            '_WIN32_WINNT=0x0600',
            'EIO_STACKSIZE=262144',
            '_GNU_SOURCE',
            'EVPIPE_OSFD',
            'UDT_EXPORTS'
          ],
          'sources': [
            'include/uv-private/uv-win.h',
            'src/ares/config_win32/ares_config.h',
            'src/ares/windows_port.c',
            'src/ares/ares_getenv.c',
            'src/ares/ares_iphlpapi.h',
            'src/ares/ares_platform.c',
            'src/win/async.c',
            'src/win/core.c',
            'src/win/dl.c',
            'src/win/error.c',
            'src/win/fs.c',
            'src/win/fs-event.c',
            'src/win/getaddrinfo.c',
            'src/win/handle.c',
            'src/win/handle-inl.h',
            'src/win/internal.h',
            'src/win/loop-watcher.c',
            'src/win/pipe.c',
            'src/win/thread.c',
            'src/win/poll.c',
            'src/win/process.c',
            'src/win/process-stdio.c',
            'src/win/req.c',
            'src/win/req-inl.h',
            'src/win/stream.c',
            'src/win/stream-inl.h',
            'src/win/tcp.c',
            'src/win/tty.c',
            'src/win/threadpool.c',
            'src/win/timer.c',
            'src/win/udp.c',
            'src/win/udt.c',
            'src/win/util.c',
            'src/win/winapi.c',
            'src/win/winapi.h',
            'src/win/winsock.c',
            'src/win/winsock.h',
          ],
          'link_settings': {
            'libraries': [
              '-ladvapi32.lib',
              '-liphlpapi.lib',
              '-lpsapi.lib',
              '-lshell32.lib',
              '-lws2_32.lib'
            ],
          },
        }, { # Not Windows i.e. POSIX
          'cflags': [
            '-g',
            '--std=gnu99',
            '-pedantic',
            '-Wall',
            '-Wextra',
            '-Wno-unused-parameter'
            '-finline-functions',
            '-fno-strict-aliasing',
            '-fvisibility=hidden',
            '-DEVPIPE_OSFD',
            '-frtti',
            '-fexceptions',
          ],
          'sources': [
            'include/uv-private/eio.h',
            'include/uv-private/ev.h',
            'include/uv-private/uv-unix.h',
            'src/unix/async.c',
            'src/unix/core.c',
            'src/unix/dl.c',
            'src/unix/eio/ecb.h',
            'src/unix/eio/eio.c',
            'src/unix/eio/xthread.h',
            'src/unix/error.c',
            'src/unix/ev/ev.c',
            'src/unix/ev/ev_vars.h',
            'src/unix/ev/ev_wrap.h',
            'src/unix/ev/event.h',
            'src/unix/fs.c',
            'src/unix/internal.h',
            'src/unix/loop.c',
            'src/unix/loop-watcher.c',
            'src/unix/pipe.c',
            'src/unix/poll.c',
            'src/unix/process.c',
            'src/unix/stream.c',
            'src/unix/tcp.c',
            'src/unix/thread.c',
            'src/unix/timer.c',
            'src/unix/tty.c',
            'src/unix/udp.c',
            'src/unix/uv-eio.c',
            'src/unix/uv-eio.h',
            'src/unix/udt.c',
          ],
          'include_dirs': [ 'src/unix/ev', ],
          'link_settings': {
            'libraries': [ 
              '-lm', 
              '-lstdc++', 
            ],
            'conditions': [
              ['OS=="solaris"', {
                'ldflags': [ '-pthreads' ],
              }, {
                'ldflags': [ '-pthread' ],
              }],
            ],
          },
        }],
        [ 'OS=="mac"', {
          'include_dirs': [ 'src/ares/config_darwin' ],
          'sources': [ 'src/unix/darwin.c' ],
          'link_settings': {
            'libraries': [
              '$(SDKROOT)/System/Library/Frameworks/CoreServices.framework',
            ],
          },
          'defines': [
            '_DARWIN_USE_64_BIT_INODE=1',
            'EV_CONFIG_H="config_darwin.h"',
            'EIO_CONFIG_H="config_darwin.h"',
            'EVPIPE_OSFD=1',
            'OSX=1',
          ]
        }],
        [ 'OS=="linux"', {
          'include_dirs': [ 'src/ares/config_linux' ],
          'sources': [
            'src/unix/linux/linux-core.c',
            'src/unix/linux/inotify.c',
            'src/unix/linux/syscalls.c',
            'src/unix/linux/syscalls.h',
          ],
          'defines': [
            'EV_CONFIG_H="config_linux.h"',
            'EIO_CONFIG_H="config_linux.h"',
            'LINUX=1'
          ],
          'link_settings': {
            'libraries': [ '-ldl', '-lrt' ],
          },
        }],
        [ 'OS=="solaris"', {
          'include_dirs': [ 'src/ares/config_sunos' ],
          'sources': [ 'src/unix/sunos.c' ],
          'defines': [
            '__EXTENSIONS__',
            '_XOPEN_SOURCE=500',
            'EV_CONFIG_H="config_sunos.h"',
            'EIO_CONFIG_H="config_sunos.h"',
          ],
          'link_settings': {
            'libraries': [
              '-lkstat',
              '-lsocket',
              '-lnsl',
            ],
          },
        }],
        [ 'OS=="freebsd"', {
          'include_dirs': [ 'src/ares/config_freebsd' ],
          'sources': [ 'src/unix/freebsd.c' ],
          'defines': [
            'EV_CONFIG_H="config_freebsd.h"',
            'EIO_CONFIG_H="config_freebsd.h"',
          ],
          'link_settings': {
            'libraries': [
              '-lkvm',
            ],
          },
        }],
        [ 'OS=="openbsd"', {
          'include_dirs': [ 'src/ares/config_openbsd' ],
          'sources': [ 'src/unix/openbsd.c' ],
          'defines': [
            'EV_CONFIG_H="config_openbsd.h"',
            'EIO_CONFIG_H="config_openbsd.h"',
          ],
        }],
        [ 'OS=="mac" or OS=="freebsd" or OS=="openbsd" or OS=="netbsd"', {
          'sources': [ 'src/unix/kqueue.c' ],
        }],
      ]
    },

    {
      'target_name': 'run-tests',
      'type': 'executable',
      'dependencies': [ 'uv' ],
      'sources': [
        'test/blackhole-server.c',
        'test/echo-server.c',
        'test/run-tests.c',
        'test/runner.c',
        'test/runner.h',
        'test/test-get-loadavg.c',
        'test/task.h',
        'test/test-util.c',
        'test/test-async.c',
        'test/test-error.c',
        'test/test-callback-stack.c',
        'test/test-callback-order.c',
        'test/test-connection-fail.c',
        'test/test-cwd-and-chdir.c',
        'test/test-delayed-accept.c',
        'test/test-fail-always.c',
        'test/test-fs.c',
        'test/test-fs-event.c',
        'test/test-get-currentexe.c',
        'test/test-get-memory.c',
        'test/test-getaddrinfo.c',
        'test/test-gethostbyname.c',
        'test/test-getsockname.c',
        'test/test-hrtime.c',
        'test/test-idle.c',
        'test/test-ipc.c',
        'test/test-ipc-send-recv.c',
        'test/test-list.h',
        'test/test-loop-handles.c',
        'test/test-walk-handles.c',
        'test/test-multiple-listen.c',
        'test/test-pass-always.c',
        'test/test-ping-pong.c',
        'test/test-pipe-bind-error.c',
        'test/test-pipe-connect-error.c',
        'test/test-platform-output.c',
        'test/test-poll.c',
        'test/test-poll-close.c',
        'test/test-process-title.c',
        'test/test-ref.c',
        'test/test-run-once.c',
        'test/test-semaphore.c',
        'test/test-shutdown-close.c',
        'test/test-shutdown-eof.c',
        'test/test-spawn.c',
        'test/test-fs-poll.c',
        'test/test-stdio-over-pipes.c',
        'test/test-tcp-bind-error.c',
        'test/test-tcp-bind6-error.c',
        'test/test-tcp-close.c',
        'test/test-tcp-close-while-connecting.c',
        'test/test-tcp-connect-error-after-write.c',
        'test/test-tcp-shutdown-after-write.c',
        'test/test-tcp-flags.c',
        'test/test-tcp-connect-error.c',
        'test/test-tcp-connect-timeout.c',
        'test/test-tcp-connect6-error.c',
        'test/test-tcp-write-error.c',
        'test/test-tcp-write-to-half-open-connection.c',
        'test/test-tcp-writealot.c',
        'test/test-tcp-unexpected-read.c',
        'test/test-threadpool.c',
        'test/test-mutexes.c',
        'test/test-thread.c',
        'test/test-timer-again.c',
        'test/test-timer.c',
        'test/test-tty.c',
        'test/test-udp-dgram-too-big.c',
        'test/test-udp-ipv6.c',
        'test/test-udp-options.c',
        'test/test-udp-send-and-recv.c',
        'test/test-udp-multicast-join.c',
        'test/test-counters-init.c',
        'test/test-dlerror.c',
        'test/test-udp-multicast-ttl.c',
      ],
      'conditions': [
        [ 'OS=="win"', {
          'sources': [
            'test/runner-win.c',
            'test/runner-win.h'
          ],
          'libraries': [ 'ws2_32.lib' ]
        }, { # POSIX
          'defines': [ '_GNU_SOURCE' ],
          'sources': [
            'test/runner-unix.c',
            'test/runner-unix.h',
          ],
        }],
        [ 'OS=="solaris"', { # make test-fs.c compile, needs _POSIX_C_SOURCE
          'defines': [
            '__EXTENSIONS__',
            '_XOPEN_SOURCE=500',
          ],
        }],
      ],
      'msvs-settings': {
        'VCLinkerTool': {
          'SubSystem': 1, # /subsystem:console
        },
      },
    },

    {
      'target_name': 'echo-server-udt',
      'type': 'executable',
      'dependencies': [ 'uv' ],
      'sources': [
        'test/echo-server-udt.c',
        'test/runner.h',
        'test/task.h',
	  ],
      'conditions': [
        [ 'OS=="win"', {
          'sources': [
            'test/runner-win.h',
          ],
          'libraries': [ 'ws2_32.lib', 'wsock32.lib' ]
        }, { # POSIX
          'defines': [ '_GNU_SOURCE' ],
          'sources': [
            'test/runner-unix.h',
          ]
        }]
      ],
      'msvs-settings': {
        'VCLinkerTool': {
          'SubSystem': 1, # /subsystem:console
        },
      },
	},

	{
      'target_name': 'echo-client-udt',
      'type': 'executable',
      'dependencies': [ 'uv' ],
      'sources': [
        'test/echo-client-udt.c',
        'test/runner.h',
        'test/task.h',
	  ],
      'conditions': [
        [ 'OS=="win"', {
          'sources': [
            'test/runner-win.h',
          ],
          'libraries': [ 'ws2_32.lib', 'wsock32.lib' ]
        }, { # POSIX
          'defines': [ '_GNU_SOURCE' ],
          'sources': [
            'test/runner-unix.h',
          ]
        }]
      ],
      'msvs-settings': {
        'VCLinkerTool': {
          'SubSystem': 1, # /subsystem:console
        },
      },
    },

    {
      'target_name': 'run-benchmarks',
      'type': 'executable',
      'dependencies': [ 'uv' ],
      'sources': [
        'test/benchmark-ares.c',
        'test/benchmark-fs-stat.c',
        'test/benchmark-getaddrinfo.c',
        'test/benchmark-list.h',
        'test/benchmark-loop-count.c',
        'test/benchmark-million-timers.c',
        'test/benchmark-ping-pongs.c',
        'test/benchmark-pound.c',
        'test/benchmark-pump.c',
        'test/benchmark-sizes.c',
        'test/benchmark-spawn.c',
        'test/benchmark-thread.c',
        'test/benchmark-tcp-write-batch.c',
        'test/benchmark-udp-packet-storm.c',
        'test/dns-server.c',
        'test/echo-server.c',
        'test/blackhole-server.c',
        'test/run-benchmarks.c',
        'test/runner.c',
        'test/runner.h',
        'test/task.h',
      ],
      'conditions': [
        [ 'OS=="win"', {
          'sources': [
            'test/runner-win.c',
            'test/runner-win.h',
          ],
          'libraries': [ 'ws2_32.lib' ]
        }, { # POSIX
          'defines': [ '_GNU_SOURCE' ],
          'sources': [
            'test/runner-unix.c',
            'test/runner-unix.h',
          ]
        }]
      ],
      'msvs-settings': {
        'VCLinkerTool': {
          'SubSystem': 1, # /subsystem:console
        },
      },
    }
  ]
}


