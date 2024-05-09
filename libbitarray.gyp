{
  'targets': [{
    'target_name': 'libbitarray',
    'type': 'static_library',
    'include_dirs': [
      './include',
    ],
    'dependencies': [
      './vendor/libintrusive/libintrusive.gyp:libintrusive',
      './vendor/libquickbit/libquickbit.gyp:libquickbit',
    ],
    'sources': [
      './src/bitarray.c',
    ],
    'direct_dependent_settings': {
      'include_dirs': [
        './include',
      ],
    },
    'export_dependent_settings': [
      './vendor/libintrusive/libintrusive.gyp:libintrusive',
      './vendor/libquickbit/libquickbit.gyp:libquickbit',
    ],
    'configurations': {
      'Debug': {
        'defines': ['DEBUG'],
      },
      'Release': {
        'defines': ['NDEBUG'],
      },
    },
  }]
}
