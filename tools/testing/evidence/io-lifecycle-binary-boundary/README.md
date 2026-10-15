# Executable containment regression

Independent source review found that the existing binary identity branch
checked a lexical build prefix. A regular executable at
`build/../foreign/nexus_provider_test` was accepted although its canonical
path was outside the selected build. This is a tool evidence-boundary defect,
not a production provider or hardware failure.

The genuine regression first ran all 26 matrix tool cases and failed exactly
one: capture did not reject the escaped executable. The minimum production
change compares canonical executable/build paths before hashing; existing
symlink, missing-input and object/source guards remain enabled. All 26 matrix
cases and four existing gate integration cases then passed with the two
maintained source identities unchanged.

The manifest preserves RED/GREEN logs, actual commands and source capsules.
The separate 203-property/710-case development execution and original 80-item
archive remain historical bytes. They do not qualify this later source;
root's final clean complete gate will bind the current tool and configuration.
No physical or formal qualification is claimed.
