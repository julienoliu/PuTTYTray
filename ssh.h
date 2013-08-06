#include <stdio.h>
#include <string.h>

#include "puttymem.h"
#include "tree234.h"
#include "network.h"
#include "int64.h"
#include "misc.h"

struct ssh_channel;

extern int sshfwd_write(struct ssh_channel *c, char *, int);
extern void sshfwd_write_eof(struct ssh_channel *c);
extern void sshfwd_unclean_close(struct ssh_channel *c);
extern void sshfwd_unthrottle(struct ssh_channel *c, int bufsize);

/*
 * Useful thing.
 */
#ifndef lenof
#define lenof(x) ((sizeof((x))) / (sizeof(*(x))))
#endif

#define SSH_CIPHER_IDEA 1
#define SSH_CIPHER_DES 2
#define SSH_CIPHER_3DES 3
#define SSH_CIPHER_BLOWFISH 6

#ifdef MSCRYPTOAPI
#define APIEXTRA 8
#else
#define APIEXTRA 0
#endif

#ifndef BIGNUM_INTERNAL
typedef void *Bignum;
#endif

struct RSAKey {
  int bits;
  int bytes;
#ifdef MSCRYPTOAPI
  unsigned long exponent;
  unsigned char *modulus;
#else
  Bignum modulus;
  Bignum exponent;
  Bignum private_exponent;
  Bignum p;
  Bignum q;
  Bignum iqmp;
#endif
  char *comment;
};

struct dss_key {
  Bignum p, q, g, y, x;
};

int makekey(unsigned char *data,
            int len,
            struct RSAKey *result,
            unsigned char **keystr,
            int order);
int makeprivate(unsigned char *data, int len, struct RSAKey *result);
int rsaencrypt(unsigned char *data, int length, struct RSAKey *key);
Bignum rsadecrypt(Bignum input, struct RSAKey *key);
void rsasign(unsigned char *data, int length, struct RSAKey *key);
void rsasanitise(struct RSAKey *key);
int rsastr_len(struct RSAKey *key);
void rsastr_fmt(char *str, struct RSAKey *key);
void rsa_fingerprint(char *str, int len, struct RSAKey *key);
int rsa_verify(struct RSAKey *key);
unsigned char *rsa_public_blob(struct RSAKey *key, int *len);
int rsa_public_blob_len(void *data, int maxlen);
void freersakey(struct RSAKey *key);

#ifndef PUTTY_UINT32_DEFINED
/* This makes assumptions about the int type. */
typedef unsigned int uint32;
#define PUTTY_UINT32_DEFINED
#endif
typedef uint32 word32;

unsigned long crc32_compute(const void *s, size_t len);
unsigned long crc32_update(unsigned long crc_input, const void *s, size_t len);

/* SSH CRC compensation attack detector */
void *crcda_make_context(void);
void crcda_free_context(void *handle);
int detect_attack(void *handle,
                  unsigned char *buf,
                  uint32 len,
                  unsigned char *IV);

/*
 * SSH2 RSA key exchange functions
 */
struct ssh_hash;
void *ssh_rsakex_newkey(char *data, int len);
void ssh_rsakex_freekey(void *key);
int ssh_rsakex_klen(void *key);
void ssh_rsakex_encrypt(const struct ssh_hash *h,
                        unsigned char *in,
                        int inlen,
                        unsigned char *out,
                        int outlen,
                        void *key);

typedef struct {
  uint32 h[4];
} MD5_Core_State;

struct MD5Context {
#ifdef MSCRYPTOAPI
  unsigned long hHash;
#else
  MD5_Core_State core;
  unsigned char block[64];
  int blkused;
  uint32 lenhi, lenlo;
#endif
};

void MD5Init(struct MD5Context *context);
void MD5Update(struct MD5Context *context,
               unsigned char const *buf,
               unsigned len);
void MD5Final(unsigned char digest[16], struct MD5Context *context);
void MD5Simple(void const *p, unsigned len, unsigned char output[16]);

void *hmacmd5_make_context(void);
void hmacmd5_free_context(void *handle);
void hmacmd5_key(void *handle, void const *key, int len);
void hmacmd5_do_hmac(void *handle,
                     unsigned char const *blk,
                     int len,
                     unsigned char *hmac);

typedef struct {
  uint32 h[5];
  unsigned char block[64];
  int blkused;
  uint32 lenhi, lenlo;
} SHA_State;
void SHA_Init(SHA_State *s);
void SHA_Bytes(SHA_State *s, const void *p, int len);
void SHA_Final(SHA_State *s, unsigned char *output);
void SHA_Simple(const void *p, int len, unsigned char *output);

void hmac_sha1_simple(
    void *key, int keylen, void *data, int datalen, unsigned char *output);
typedef struct {
  uint32 h[8];
  unsigned char block[64];
  int blkused;
  uint32 lenhi, lenlo;
} SHA256_State;
void SHA256_Init(SHA256_State *s);
void SHA256_Bytes(SHA256_State *s, const void *p, int len);
void SHA256_Final(SHA256_State *s, unsigned char *output);
void SHA256_Simple(const void *p, int len, unsigned char *output);

typedef struct {
  uint64 h[8];
  unsigned char block[128];
  int blkused;
  uint32 len[4];
} SHA512_State;
void SHA512_Init(SHA512_State *s);
void SHA512_Bytes(SHA512_State *s, const void *p, int len);
void SHA512_Final(SHA512_State *s, unsigned char *output);
void SHA512_Simple(const void *p, int len, unsigned char *output);

struct ssh_cipher {
  void *(*make_context)(void);
  void (*free_context)(void *);
  void (*sesskey)(void *, unsigned char *key); /* for SSH-1 */
  void (*encrypt)(void *, unsigned char *blk, int len);
  void (*decrypt)(void *, unsigned char *blk, int len);
  int blksize;
  char *text_name;
};

struct ssh2_cipher {
  void *(*make_context)(void);
  void (*free_context)(void *);
  void (*setiv)(void *, unsigned char *key);  /* for SSH-2 */
  void (*setkey)(void *, unsigned char *key); /* for SSH-2 */
  void (*encrypt)(void *, unsigned char *blk, int len);
  void (*decrypt)(void *, unsigned char *blk, int len);
  char *name;
  int blksize;
  int keylen;
  unsigned int flags;
#define SSH_CIPHER_IS_CBC 1
  char *text_name;
};

struct ssh2_ciphers {
  int nciphers;
  const struct ssh2_cipher *const *list;
};

struct ssh_mac {
  void *(*make_context)(void);
  void (*free_context)(void *);
  void (*setkey)(void *, unsigned char *key);
  /* whole-packet operations */
  void (*generate)(void *, unsigned char *blk, int len, unsigned long seq);
  int (*verify)(void *, unsigned char *blk, int len, unsigned long seq);
  /* partial-packet operations */
  void (*start)(void *);
  void (*bytes)(void *, unsigned char const *, int);
  void (*genresult)(void *, unsigned char *);
  int (*verresult)(void *, unsigned char const *);
  char *name;
  int len;
  char *text_name;
};

struct ssh_hash {
  void *(*init)(void); /* also allocates context */
  void (*bytes)(void *, void *, int);
  void (*final)(void *, unsigned char *); /* also frees context */
  int hlen;                               /* output length in bytes */
  char *text_name;
};

struct ssh_kex {
  char *name, *groupname;
  enum
  {
    KEXTYPE_DH,
    KEXTYPE_RSA
  } main_type;
  /* For DH */
  const unsigned char *pdata, *gdata; /* NULL means group exchange */
  int plen, glen;
  const struct ssh_hash *hash;
};

struct ssh_kexes {
  int nkexes;
  const struct ssh_kex *const *list;
};

struct ssh_signkey {
  void *(*newkey)(char *data, int len);
  void (*freekey)(void *key);
  char *(*fmtkey)(void *key);
  unsigned char *(*public_blob)(void *key, int *len);
  unsigned char *(*private_blob)(void *key, int *len);
  void *(*createkey)(unsigned char *pub_blob,
                     int pub_len,
                     unsigned char *priv_blob,
                     int priv_len);
  void *(*openssh_createkey)(unsigned char **blob, int *len);
  int (*openssh_fmtkey)(void *key, unsigned char *blob, int len);
  int (*pubkey_bits)(void *blob, int len);
  char *(*fingerprint)(void *key);
  int (*verifysig)(void *key, char *sig, int siglen, char *data, int datalen);
  unsigned char *(*sign)(void *key, char *data, int datalen, int *siglen);
  char *name;
  char *keytype; /* for host key cache */
};

struct ssh_compress {
  char *name;
  /* For zlib@openssh.com: if non-NULL, this name will be considered once
   * userauth has completed successfully. */
  char *delayed_name;
  void *(*compress_init)(void);
  void (*compress_cleanup)(void *);
  int (*compress)(void *,
                  unsigned char *block,
                  int len,
                  unsigned char **outblock,
                  int *outlen);
  void *(*decompress_init)(void);
  void (*decompress_cleanup)(void *);
  int (*decompress)(void *,
                    unsigned char *block,
                    int len,
                    unsigned char **outblock,
                    int *outlen);
  int (*disable_compression)(void *);
  char *text_name;
};

struct ssh2_userkey {
  const struct ssh_signkey *alg; /* the key algorithm */
  void *data;                    /* the key data */
  char *comment;                 /* the key comment */
};

/* The maximum length of any hash algorithm used in kex. (bytes) */
#define SSH2_KEX_MAX_HASH_LEN (32) /* SHA-256 */

extern const struct ssh_cipher ssh_3des;
extern const struct ssh_cipher ssh_des;
extern const struct ssh_cipher ssh_blowfish_ssh1;
extern const struct ssh2_ciphers ssh2_3des;
extern const struct ssh2_ciphers ssh2_des;
extern const struct ssh2_ciphers ssh2_aes;
extern const struct ssh2_ciphers ssh2_blowfish;
extern const struct ssh2_ciphers ssh2_arcfour;
extern const struct ssh_hash ssh_sha1;
extern const struct ssh_hash ssh_sha256;
extern const struct ssh_kexes ssh_diffiehellman_group1;
extern const struct ssh_kexes ssh_diffiehellman_group14;
extern const struct ssh_kexes ssh_diffiehellman_gex;
extern const struct ssh_kexes ssh_rsa_kex;
extern const struct ssh_signkey ssh_dss;
extern const struct ssh_signkey ssh_rsa;
extern const struct ssh_mac ssh_hmac_md5;
extern const struct ssh_mac ssh_hmac_sha1;
extern const struct ssh_mac ssh_hmac_sha1_buggy;
extern const struct ssh_mac ssh_hmac_sha1_96;
extern const struct ssh_mac ssh_hmac_sha1_96_buggy;
extern const struct ssh_mac ssh_hmac_sha256;

void *aes_make_context(void);
void aes_free_context(void *handle);
void aes128_key(void *handle, unsigned char *key);
void aes192_key(void *handle, unsigned char *key);
void aes256_key(void *handle, unsigned char *key);
void aes_iv(void *handle, unsigned char *iv);
void aes_ssh2_encrypt_blk(void *handle, unsigned char *blk, int len);
void aes_ssh2_decrypt_blk(void *handle, unsigned char *blk, int len);

/*
 * PuTTY version number formatted as an SSH version string.
 */
extern char sshver[];

/*
 * Gross hack: pscp will try to start SFTP but fall back to scp1 if
 * that fails. This variable is the means by which scp.c can reach
 * into the SSH code and find out which one it got.
 */
extern int ssh_fallback_cmd(void *handle);

#ifndef MSCRYPTOAPI
void SHATransform(word32 *digest, word32 *data);
#endif

int random_byte(void);
void random_add_noise(void *noise, int length);
void random_add_heavynoise(void *noise, int length);

void logevent(void *, const char *);

/* Allocate and register a new channel for port forwarding */
void *new_sock_channel(void *handle, Socket s);
void ssh_send_port_open(void *channel, char *hostname, int port, char *org);

/* Exports from portfwd.c */
extern const char *pfd_newconnect(Socket *s,
                                  char *hostname,
                                  int port,
                                  void *c,
                                  Conf *conf,
                                  int addressfamily);
/* desthost == NULL indicates dynamic (SOCKS) port forwarding */
extern const char *pfd_addforward(char *desthost,
                                  int destport,
                                  char *srcaddr,
                                  int port,
                                  void *backhandle,
                                  Conf *conf,
                                  void **sockdata,
                                  int address_family);
extern void pfd_close(Socket s);
extern void pfd_terminate(void *sockdata);
extern int pfd_send(Socket s, char *data, int len);
extern void pfd_send_eof(Socket s);
extern void pfd_confirm(Socket s);
extern void pfd_unthrottle(Socket s);
extern void pfd_override_throttle(Socket s, int enable);

/* Exports from x11fwd.c */
enum
{
  X11_TRANS_IPV4 = 0,
  X11_TRANS_IPV6 = 6,
  X11_TRANS_UNIX = 256
};
struct X11Display {
  /* Broken-down components of the display name itself */
  int unixdomain;
  char *hostname;
  int displaynum;
  int screennum;
  /* OSX sometimes replaces all the above with a full Unix-socket pathname */
  char *unixsocketpath;

  /* PuTTY networking SockAddr to connect to the display, and associated
   * gubbins */
  SockAddr addr;
  int port;
  char *realhost;

  /* Auth details we invented for the virtual display on the SSH server. */
  int remoteauthproto;
  unsigned char *remoteauthdata;
  int remoteauthdatalen;
  char *remoteauthprotoname;
  char *remoteauthdatastring;

  /* Our local auth details for talking to the real X display. */
  int localauthproto;
  unsigned char *localauthdata;
  int localauthdatalen;

  /*
   * Used inside x11fwd.c to remember recently seen
   * XDM-AUTHORIZATION-1 strings, to avoid replay attacks.
   */
  tree234 *xdmseen;
};
/*
 * x11_setup_display() parses the display variable and fills in an
 * X11Display structure. Some remote auth details are invented;
 * the supplied authtype parameter configures the preferred
 * authorisation protocol to use at the remote end. The local auth
 * details are looked up by calling platform_get_x11_auth.
 */
extern struct X11Display *x11_setup_display(char *display,
                                            int authtype,
                                            Conf *);
void x11_free_display(struct X11Display *disp);
extern const char *x11_init(
    Socket *, struct X11Display *, void *, const char *, int, Conf *);
extern void x11_close(Socket);
extern int x11_send(Socket, char *, int);
extern void x11_send_eof(Socket s);
extern void x11_unthrottle(Socket s);
extern void x11_override_throttle(Socket s, int enable);
char *x11_display(const char *display);
/* Platform-dependent X11 functions */
extern void platform_get_x11_auth(struct X11Display *display, Conf *);
/* examine a mostly-filled-in X11Display and fill in localauth* */
extern const int platform_uses_x11_unix_by_default;
/* choose default X transport in the absence of a specified one */
SockAddr platform_get_x11_unix_address(const char *path, int displaynum);
/* make up a SockAddr naming the address for displaynum */
char *platform_get_x_display(void);
/* allocated local X display string, if any */
/* Callbacks in x11.c usable _by_ platform X11 functions */
/*
 * This function does the job of platform_get_x11_auth, provided
 * it is told where to find a normally formatted .Xauthority file:
 * it opens that file, parses it to find an auth record which
 * matches the display details in "display", and fills in the
 * localauth fields.
 *
 * It is expected that most implementations of
 * platform_get_x11_auth() will work by finding their system's
 * .Xauthority file, adjusting the display details if necessary
 * for local oddities like Unix-domain socket transport, and
 * calling this function to do the rest of the work.
 */
void x11_get_auth_from_authfile(struct X11Display *display,
                                const char *authfilename);

Bignum copybn(Bignum b);
Bignum bn_power_2(int n);
void bn_restore_invariant(Bignum b);
Bignum bignum_from_long(unsigned long n);
void freebn(Bignum b);
Bignum modpow(Bignum base, Bignum exp, Bignum mod);
Bignum modmul(Bignum a, Bignum b, Bignum mod);
void decbn(Bignum n);
extern Bignum Zero, One;
Bignum bignum_from_bytes(const unsigned char *data, int nbytes);
int ssh1_read_bignum(const unsigned char *data, int len, Bignum *result);
int bignum_bitcount(Bignum bn);
int ssh1_bignum_length(Bignum bn);
int ssh2_bignum_length(Bignum bn);
int bignum_byte(Bignum bn, int i);
int bignum_bit(Bignum bn, int i);
void bignum_set_bit(Bignum bn, int i, int value);
int ssh1_write_bignum(void *data, Bignum bn);
Bignum biggcd(Bignum a, Bignum b);
unsigned short bignum_mod_short(Bignum number, unsigned short modulus);
Bignum bignum_add_long(Bignum number, unsigned long addend);
Bignum bigadd(Bignum a, Bignum b);
Bignum bigsub(Bignum a, Bignum b);
Bignum bigmul(Bignum a, Bignum b);
Bignum bigmuladd(Bignum a, Bignum b, Bignum addend);
Bignum bigdiv(Bignum a, Bignum b);
Bignum bigmod(Bignum a, Bignum b);
Bignum modinv(Bignum number, Bignum modulus);
Bignum bignum_bitmask(Bignum number);
Bignum bignum_rshift(Bignum number, int shift);
int bignum_cmp(Bignum a, Bignum b);
char *bignum_decimal(Bignum x);

#ifdef DEBUG
void diagbn(char *prefix, Bignum md);
#endif

void *dh_setup_group(const struct ssh_kex *kex);
void *dh_setup_gex(Bignum pval, Bignum gval);
void dh_cleanup(void *);
Bignum dh_create_e(void *, int nbits);
Bignum dh_find_K(void *, Bignum f);

int loadrsakey(const Filename *filename,
               struct RSAKey *key,
               char *passphrase,
               const char **errorstr);
int rsakey_encrypted(const Filename *filename, char **comment);
int rsakey_pubblob(const Filename *filename,
                   void **blob,
                   int *bloblen,
                   char **commentptr,
                   const char **errorstr);

int saversakey(const Filename *filename, struct RSAKey *key, char *passphrase);

extern int base64_decode_atom(char *atom, unsigned char *out);
extern int base64_lines(int datalen);
extern void base64_encode_atom(unsigned char *data, int n, char *out);
extern void base64_encode(FILE *fp, unsigned char *data, int datalen, int cpl);

/* ssh2_load_userkey can return this as an error */
extern struct ssh2_userkey ssh2_wrong_passphrase;
#define SSH2_WRONG_PASSPHRASE (&ssh2_wrong_passphrase)

int ssh2_userkey_encrypted(const Filename *filename, char **comment);
struct ssh2_userkey *ssh2_load_userkey(const Filename *filename,
                                       char *passphrase,
                                       const char **errorstr);
unsigned char *ssh2_userkey_loadpub(const Filename *filename,
                                    char **algorithm,
                                    int *pub_blob_len,
                                    char **commentptr,
                                    const char **errorstr);
int ssh2_save_userkey(const Filename *filename,
                      struct ssh2_userkey *key,
                      char *passphrase);
const struct ssh_signkey *find_pubkey_alg(const char *name);

enum
{
  SSH_KEYTYPE_UNOPENABLE,
  SSH_KEYTYPE_UNKNOWN,
  SSH_KEYTYPE_SSH1,
  SSH_KEYTYPE_SSH2,
  SSH_KEYTYPE_OPENSSH,
  SSH_KEYTYPE_SSHCOM
};
int key_type(const Filename *filename);
char *key_type_to_str(int type);

int import_possible(int type);
int import_target_type(int type);
int import_encrypted(const Filename *filename, int type, char **comment);
int import_ssh1(const Filename *filename,
                int type,
                struct RSAKey *key,
                char *passphrase,
                const char **errmsg_p);
struct ssh2_userkey *import_ssh2(const Filename *filename,
                                 int type,
                                 char *passphrase,
                                 const char **errmsg_p);
int export_ssh1(const Filename *filename,
                int type,
                struct RSAKey *key,
                char *passphrase);
int export_ssh2(const Filename *filename,
                int type,
                struct ssh2_userkey *key,
                char *passphrase);

void des3_decrypt_pubkey(unsigned char *key, unsigned char *blk, int len);
void des3_encrypt_pubkey(unsigned char *key, unsigned char *blk, int len);
void des3_decrypt_pubkey_ossh(unsigned char *key,
                              unsigned char *iv,
                              unsigned char *blk,
                              int len);
void des3_encrypt_pubkey_ossh(unsigned char *key,
                              unsigned char *iv,
                              unsigned char *blk,
                              int len);
void aes256_encrypt_pubkey(unsigned char *key, unsigned char *blk, int len);
void aes256_decrypt_pubkey(unsigned char *key, unsigned char *blk, int len);

void des_encrypt_xdmauth(unsigned char *key, unsigned char *blk, int len);
void des_decrypt_xdmauth(unsigned char *key, unsigned char *blk, int len);

/*
 * For progress updates in the key generation utility.
 */
#define PROGFN_INITIALISE 1
#define PROGFN_LIN_PHASE 2
#define PROGFN_EXP_PHASE 3
#define PROGFN_PHASE_EXTENT 4
#define PROGFN_READY 5
#define PROGFN_PROGRESS 6
typedef void (*progfn_t)(void *param, int action, int phase, int progress);

int rsa_generate(struct RSAKey *key, int bits, progfn_t pfn, void *pfnparam);
int dsa_generate(struct dss_key *key, int bits, progfn_t pfn, void *pfnparam);
Bignum primegen(int bits,
                int modulus,
                int residue,
                Bignum factor,
                int phase,
                progfn_t pfn,
                void *pfnparam,
                unsigned firstbits);
void invent_firstbits(unsigned *one, unsigned *two);

/*
 * zlib compression.
 */
void *zlib_compress_init(void);
void zlib_compress_cleanup(void *);
void *zlib_decompress_init(void);
void zlib_decompress_cleanup(void *);
int zlib_compress_block(void *,
                        unsigned char *block,
                        int len,
                        unsigned char **outblock,
                        int *outlen);
int zlib_decompress_block(void *,
                          unsigned char *block,
                          int len,
                          unsigned char **outblock,
                          int *outlen);

/*
 * SSH-1 agent messages.
 */
#define SSH1_AGENTC_REQUEST_RSA_IDENTITIES 1
#define SSH1_AGENT_RSA_IDENTITIES_ANSWER 2
#define SSH1_AGENTC_RSA_CHALLENGE 3
#define SSH1_AGENT_RSA_RESPONSE 4
#define SSH1_AGENTC_ADD_RSA_IDENTITY 7
#define SSH1_AGENTC_REMOVE_RSA_IDENTITY 8
#define SSH1_AGENTC_REMOVE_ALL_RSA_IDENTITIES 9 /* openssh private? */

/*
 * Messages common to SSH-1 and OpenSSH's SSH-2.
 */
#define SSH_AGENT_FAILURE 5
#define SSH_AGENT_SUCCESS 6

/*
 * OpenSSH's SSH-2 agent messages.
 */
#define SSH2_AGENTC_REQUEST_IDENTITIES 11
#define SSH2_AGENT_IDENTITIES_ANSWER 12
#define SSH2_AGENTC_SIGN_REQUEST 13
#define SSH2_AGENT_SIGN_RESPONSE 14
#define SSH2_AGENTC_ADD_IDENTITY 17
#define SSH2_AGENTC_REMOVE_IDENTITY 18
#define SSH2_AGENTC_REMOVE_ALL_IDENTITIES 19

/*
 * Need this to warn about support for the original SSH-2 keyfile
 * format.
 */
void old_keyfile_warning(void);
