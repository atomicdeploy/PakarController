import 'package:flutter/material.dart';

// Shared Studio roles, Vazirmatn and stable geometry for every route and capture.
const studioDark = ColorScheme.dark(
  primary: Color(0xff68a7ff),
  secondary: Color(0xff38d27a),
  error: Color(0xffff5c68),
  surface: Color(0xff11161e),
  onSurface: Color(0xffeef2f7),
  onSurfaceVariant: Color(0xff909baa),
  outline: Color(0xff29313d),
  outlineVariant: Color(0xff313b49),
  surfaceContainerHigh: Color(0xff1b2330),
);

ThemeData studioTheme({Map<String, int> palette = const {}}) => ThemeData(
  fontFamily: 'Vazirmatn',
  colorScheme: studioDark.copyWith(
    primary: palette['primary'] == null
        ? null
        : Color(0xff000000 | palette['primary']!),
    secondary: palette['secondary'] == null
        ? null
        : Color(0xff000000 | palette['secondary']!),
    surface: palette['surface'] == null
        ? null
        : Color(0xff000000 | palette['surface']!),
    onSurface: palette['on_surface'] == null
        ? null
        : Color(0xff000000 | palette['on_surface']!),
  ),
  useMaterial3: true,
  scaffoldBackgroundColor: const Color(0xff080a0e),
  appBarTheme: const AppBarTheme(
    backgroundColor: Color(0xff11161e),
    surfaceTintColor: Colors.transparent,
    elevation: 0,
  ),
  cardTheme: const CardThemeData(
    color: Color(0xff11161e),
    elevation: 0,
    surfaceTintColor: Colors.transparent,
    shape: RoundedRectangleBorder(
      side: BorderSide(color: Color(0xff29313d)),
      borderRadius: BorderRadius.all(Radius.circular(12)),
    ),
  ),
  inputDecorationTheme: InputDecorationTheme(
    isDense: true,
    filled: true,
    fillColor: const Color(0xff0c1118),
    contentPadding: const EdgeInsets.symmetric(horizontal: 14, vertical: 15),
    labelStyle: const TextStyle(color: Color(0xff909baa)),
    border: OutlineInputBorder(borderRadius: BorderRadius.circular(8)),
    enabledBorder: OutlineInputBorder(
      borderRadius: BorderRadius.circular(8),
      borderSide: const BorderSide(color: Color(0xff29313d)),
    ),
    focusedBorder: OutlineInputBorder(
      borderRadius: BorderRadius.circular(8),
      borderSide: const BorderSide(color: Color(0xff68a7ff)),
    ),
  ),
  dividerTheme: const DividerThemeData(color: Color(0xff29313d), thickness: 1),
  dialogTheme: const DialogThemeData(
    backgroundColor: Color(0xff11161e),
    surfaceTintColor: Colors.transparent,
  ),
  popupMenuTheme: const PopupMenuThemeData(
    color: Color(0xff11161e),
    surfaceTintColor: Colors.transparent,
    position: PopupMenuPosition.under,
    elevation: 8,
    shape: RoundedRectangleBorder(
      side: BorderSide(color: Color(0xff29313d)),
      borderRadius: BorderRadius.all(Radius.circular(12)),
    ),
    labelTextStyle: WidgetStatePropertyAll(
      TextStyle(fontFamily: 'Vazirmatn', fontSize: 14, height: 1.25),
    ),
  ),
  filledButtonTheme: FilledButtonThemeData(
    style: FilledButton.styleFrom(
      minimumSize: const Size(0, 36),
      tapTargetSize: MaterialTapTargetSize.padded,
      shape: RoundedRectangleBorder(borderRadius: BorderRadius.circular(8)),
      padding: const EdgeInsets.symmetric(horizontal: 12, vertical: 8),
    ),
  ),
  outlinedButtonTheme: OutlinedButtonThemeData(
    style: OutlinedButton.styleFrom(
      minimumSize: const Size(0, 36),
      tapTargetSize: MaterialTapTargetSize.padded,
      foregroundColor: studioDark.onSurface,
      side: BorderSide(color: studioDark.outlineVariant),
      shape: RoundedRectangleBorder(borderRadius: BorderRadius.circular(8)),
      padding: const EdgeInsets.symmetric(horizontal: 12, vertical: 8),
    ),
  ),
  textButtonTheme: TextButtonThemeData(
    style: TextButton.styleFrom(
      minimumSize: const Size(0, 36),
      tapTargetSize: MaterialTapTargetSize.padded,
      padding: const EdgeInsets.symmetric(horizontal: 10, vertical: 6),
      shape: RoundedRectangleBorder(borderRadius: BorderRadius.circular(8)),
    ),
  ),
);
