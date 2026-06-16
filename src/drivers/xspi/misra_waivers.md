# xSPI MISRA Waivers

## Rule 11.5 - Conversion From `void *`

The fake port casts the erased `driver_ctx` pointer back to
`xSPI_Fake_Context_t *` in `xspi_fake.c`. The portable xSPI core never
dereferences `driver_ctx`; ownership and type correctness are established by
the board or test instance that binds `xSPI_Fake_Driver_Ops` to a
`xSPI_Fake_Context_t` object.

Call sites:

- `as_fake_context`
- `as_const_fake_context`
