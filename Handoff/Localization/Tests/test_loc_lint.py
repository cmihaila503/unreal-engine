# python3 -m unittest Handoff/Localization/Tests/test_loc_lint.py  (from the repo root) or run this file directly
import os
import sys
import unittest

sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', 'Tools'))
import loc_lint  # noqa: E402


class LocLint(unittest.TestCase):
    def lint(self, src):
        f, k = loc_lint.lint_text('x.cpp', src)
        return f + loc_lint.find_dups(k)

    def test_raw_display_text_is_found(self):
        r = self.lint('Model.PromptText = FText::FromString(FString::Printf(TEXT("Urcă în %s"), *Name));')
        self.assertEqual([x[0] for x in r], ['RAW'])

    def test_raw_english_words_found_too(self):
        r = self.lint('T = FText::FromString(TEXT("Press any key"));')
        self.assertEqual([x[0] for x in r], ['RAW'])

    def test_numbers_and_single_tokens_are_fine(self):
        self.assertEqual(self.lint('T = FText::FromString(FString::Printf(TEXT("%02d:%02d"), H, M));'), [])
        self.assertEqual(self.lint('T = FText::FromString(TEXT("OK"));'), [])

    def test_localized_text_is_fine(self):
        self.assertEqual(self.lint('T = NSLOCTEXT("MurdarPolice", "PullOver", "Poliția! Trage pe dreapta!");'), [])
        self.assertEqual(self.lint('#define LOCTEXT_NAMESPACE "M"\nT = LOCTEXT("Busy", "Ai deja o treabă.");'), [])

    def test_diacritic_outside_loctext(self):
        r = self.lint('Log(TEXT("Mașina a plecat"));')
        self.assertEqual([x[0] for x in r], ['DIAC'])

    def test_duplicate_key_with_different_text(self):
        src = ('A = NSLOCTEXT("Ns", "Key", "Unu");\n'
               'B = NSLOCTEXT("Ns", "Key", "Doi");\n'
               'C = NSLOCTEXT("Ns", "Key", "Unu");\n')
        r = self.lint(src)
        self.assertEqual([x[0] for x in r], ['DUP'])

    def test_loctext_namespace_from_define(self):
        src = ('#define LOCTEXT_NAMESPACE "A"\nX = LOCTEXT("K", "Unu");\n#undef LOCTEXT_NAMESPACE\n'
               '#define LOCTEXT_NAMESPACE "B"\nY = LOCTEXT("K", "Doi");\n')
        self.assertEqual(self.lint(src), [])  # same key, different namespaces

    def test_opt_out(self):
        self.assertEqual(self.lint('T = FText::FromString(TEXT("Urcă în mașină")); // loc-ok: debug overlay'), [])


if __name__ == '__main__':
    unittest.main()
