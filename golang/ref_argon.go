package main

import (
	"bytes"
	"encoding/binary"
	"encoding/hex"
	"fmt"
	"log"
	"math/big"
	"time"

	"golang.org/x/crypto/argon2"
	"golang.org/x/crypto/blake2b"
)

func Hex2Bytes(str string) []byte {
	h, _ := hex.DecodeString(str)
	return h
}

func FromHex(s string) []byte {
	if len(s) > 1 {
		if s[0:2] == "0x" || s[0:2] == "0X" {
			s = s[2:]
		}
	}
	if len(s)%2 == 1 {
		s = "0" + s
	}
	return Hex2Bytes(s)
}

const (
	HashLength = 32

// 	AddressLength = 20
)

type Hash [HashLength]byte

func BytesToHash(b []byte) Hash {
	var h Hash
	h.SetBytes(b)
	return h
}

// SetBytes sets the hash to the value of b. If b is larger than len(h), 'b' will be cropped (from the left).
func (h *Hash) SetBytes(b []byte) {
	if len(b) > len(h) {
		b = b[len(b)-HashLength:]
	}

	copy(h[HashLength-len(b):], b)
}

func (h Hash) Str() string   { return string(h[:]) }
func (h Hash) Bytes() []byte { return h[:] }
func (h Hash) Big() *big.Int { return new(big.Int).SetBytes(h[:]) }

//func (h Hash) Hex() string   { return hexutil.Encode(h[:]) }

func HexToHash(s string) Hash { return BytesToHash(FromHex(s)) }

const (
	argonThreads uint8  = 1
	argonTime    uint32 = 1
	argonMem     uint32 = 1
)

// Argon2id calculates and returns the Argon2id hash of the input data.
func Argon2id(data ...[]byte) []byte {
	return argon2idA(data...)
}

// Argon2id calculates and returns the Argon2id hash of the input data.
func argon2idA(data ...[]byte) []byte {
	buf := &bytes.Buffer{}
	for i := range data {
		buf.Write(data[i])
	}
	return argon2.IDKey(buf.Bytes(), nil, 1, argonMem, argonThreads, HashLength)
}

// Argon2id calculates and returns the Argon2id hash of the input data.
func Argon2idHash(data ...[]byte) (h Hash) {
	return BytesToHash(Argon2id(data...))
}

// The Argon2 version implemented by this package.
const Version = 0x13

const (
	argon2d = iota
	argon2i
	argon2id
)

// IDKey derives a key from the password, salt, and cost parameters using
// Argon2id returning a byte slice of length keyLen that can be used as
// cryptographic key. The CPU cost and parallelism degree must be greater than
// zero.
//
// For example, you can get a derived key for e.g. AES-256 (which needs a
// 32-byte key) by doing:
//
//      key := argon2.IDKey([]byte("some password"), salt, 1, 64*1024, 4, 32)
//
// The draft RFC recommends[2] time=1, and memory=64*1024 is a sensible number.
// If using that amount of memory (64 MB) is not possible in some contexts then
// the time parameter can be increased to compensate.
//
// The time parameter specifies the number of passes over the memory and the
// memory parameter specifies the size of the memory in KiB. For example
// memory=64*1024 sets the memory cost to ~64 MB. The number of threads can be
// adjusted to the numbers of available CPUs. The cost parameters should be
// increased as memory latency and CPU parallelism increases. Remember to get a
// good random salt.
// func IDKey(password, salt []byte, time, memory uint32, threads uint8, keyLen uint32) []byte {
// 	return deriveKey(argon2id, password, salt, nil, nil, time, memory, threads, keyLen)
// }

func initHash(password, salt, key, data []byte, time, memory, threads, keyLen uint32, mode int) [blake2b.Size + 8]byte {
	var (
		h0     [blake2b.Size + 8]byte
		params [24]byte
		tmp    [4]byte
	)

	b2, _ := blake2b.New512(nil)
	binary.LittleEndian.PutUint32(params[0:4], threads)
	binary.LittleEndian.PutUint32(params[4:8], keyLen)
	binary.LittleEndian.PutUint32(params[8:12], memory)
	binary.LittleEndian.PutUint32(params[12:16], time)
	binary.LittleEndian.PutUint32(params[16:20], uint32(Version))
	binary.LittleEndian.PutUint32(params[20:24], uint32(mode))
	b2.Write(params[:])
	binary.LittleEndian.PutUint32(tmp[:], uint32(len(password)))
	b2.Write(tmp[:])
	b2.Write(password)
	binary.LittleEndian.PutUint32(tmp[:], uint32(len(salt)))
	b2.Write(tmp[:])
	//b2.Write(salt)
	binary.LittleEndian.PutUint32(tmp[:], uint32(len(key)))
	b2.Write(tmp[:])
	//b2.Write(key)
	binary.LittleEndian.PutUint32(tmp[:], uint32(len(data)))
	b2.Write(tmp[:])
	//b2.Write(data)
	b2.Sum(h0[:0])
	return h0
}

func printBytes(name string, data []byte) {
	fmt.Printf("%s{ ", name)
	for v := range data {
		fmt.Printf("%d, ", data[v])
	}
	fmt.Printf("}\n")
}

func timeTrack(start time.Time, name string) {
	elapsed := time.Since(start)
	log.Printf("%s took %s", name, elapsed)
}

func main() {
	////////////////////////////////////////////////////////////////////////////////////
	var nonce uint64 = 5577006791947779410
	var benchdiff = new(big.Int).SetBytes(HexToHash("0x08637bd05af6c69b5a63f9a49c2c1b10fd7e45803cd141a6937d1fe64f54").Bytes())
	var benchwork = HexToHash("0xd3b5f1b47f52fdc72b1dab0b02ab352442487a1d3a43211bc4f0eb5f092403fc")

	////////////////////////////////////////////////////////////////////////////////////
	fmt.Printf("diff : %s\n", benchdiff.String())
	fmt.Printf("hash : %s\n", (benchwork.Big()).String())
	fmt.Printf("nonce: %d\n", nonce)

	// create hashing seed
	seed := make([]byte, 40)
	copy(seed, benchwork.Bytes())
	binary.LittleEndian.PutUint64(seed[32:], nonce)

	seedBuf := &bytes.Buffer{}
	for i := range seed {
		seedBuf.WriteByte(seed[i])
	}

	////////////////////////////////////////////////////////////////////////////////////
	password := seedBuf.Bytes()
	var salt []byte
	var secret []byte
	var data []byte

	fmt.Printf("\n---H0 test---\n")

	fmt.Printf("- Input -\n")
	printBytes("password   : ", password)
	fmt.Printf("salt       : %s\n", string(salt[:]))
	fmt.Printf("secret     : %s\n", string(secret[:]))
	fmt.Printf("data       : %s\n", string(data[:]))
	fmt.Printf("time       : %d\n", argonTime)
	fmt.Printf("mem        : %d\n", argonMem)
	fmt.Printf("threads    : %d\n", argonThreads)
	fmt.Printf("HashLength : %d\n", HashLength)

	// -- BENCHING BLAKE2B --
/*	
	var h0 [blake2b.Size + 8]byte
	h0 = initHash(password, salt, secret, data, argonTime, argonMem, uint32(argonThreads), HashLength, argon2i)

	{
		defer timeTrack(time.Now(), "initHash")
		for i := 0; i < 100*1000; i++ {
			h0 = initHash(password, salt, secret, data, argonTime, argonMem, uint32(argonThreads), HashLength, argon2i)
		}
	}
*/
	fmt.Printf("- Output (H0) -\n")
	printBytes("H0  :", h0[0:blake2b.Size])

	////////////////////////////////////////////////////////////////////////////////////
	fmt.Printf("\n---Argon2i test---\n")
	hashArgon2i := BytesToHash(argon2.Key(seedBuf.Bytes(), nil, 1, argonMem, argonThreads, HashLength))
	printBytes("result: ", hashArgon2i.Bytes())

	////////////////////////////////////////////////////////////////////////////////////
	fmt.Printf("\n---Argon2id test---\n")
	hashArgon2id := BytesToHash(argon2.IDKey(seedBuf.Bytes(), nil, 1, argonMem, argonThreads, HashLength))
	printBytes("result: ", hashArgon2id.Bytes())
	fmt.Printf("result as BigInt: %d\n", hashArgon2id.Big())

	fmt.Printf("\n")
}
