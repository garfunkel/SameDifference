package main

import (
	"bytes"
	"encoding/binary"
	"encoding/hex"
	"errors"
	"fmt"
	"log"
	"os"
	"os/exec"
	"path/filepath"
	"regexp"
	"strconv"
	"strings"
)

type hash [8]byte

type Fingerprint struct {
	Path string
	Fingerprint [192]byte
}

type SimilarityGroup []Fingerprint

var PGMSpec = regexp.MustCompile(`(?s)(?P<magicNumber>P5)\s(?P<width>\d+)\s(?P<height>\d+)\s(?P<maxVal>\d+)\s(?P<pixels>.*)`)
var SimilarityThreshold = uint(120)

func (fingerprint Fingerprint) calculateDistance(otherFingerprint Fingerprint) (distance uint) {
	var otherFpByte byte

	for fpByteIndex, fpByte := range fingerprint.Fingerprint {
		otherFpByte = otherFingerprint.Fingerprint[fpByteIndex]

		for digit := byte(1); digit > 0; digit *= 2 {
			if (digit & fpByte) != (digit & otherFpByte) {
				distance += 1
			}
		}
	}

	return
}

func generateHashComponent(pgm []byte) (component hash, err error) {
	match := PGMSpec.FindSubmatch(pgm)

	if match == nil {
		err = errors.New("failed to parse PGM image")

		return
	}

	width, err := strconv.Atoi(string(match[2]))

	if err != nil {
		err = errors.New("failed to extract PGM image width")

		return
	}

	height, err := strconv.Atoi(string(match[3]))

	if err != nil {
		err = errors.New("failed to extract PGM image height")

		return
	}

	maxVal, err := strconv.Atoi(string(match[4]))

	if err != nil {
		err = errors.New("failed to extract PGM image maximum value")

		return
	}

	numPixels := width * height
	pixelBytes := match[5]
	pixelWidth := 1

	if maxVal < 1 {
		err = errors.New("PGM image maximum value is not >= 1")
	} else if maxVal < 256 {
		if numPixels != len(pixelBytes) {
			err = errors.New("PGM image maximum value (1 byte) is inconsistent with number of pixel bytes")
		}
	} else if maxVal < 65536 {
		if numPixels != len(pixelBytes) * 2 {
			err = errors.New("PGM image maximum value (2 bytes) is inconsistent with number of pixel bytes")
		} else {
			pixelWidth = 2
		}
	} else {
		err = errors.New("PGM image maximum value is not < 65536")
	}

	if err != nil {
		return
	}

	var pixel uint16
	var nextPixel uint16
	componentByte := 0

	for pos := 0; pos < len(pixelBytes); pos += pixelWidth {
		if pixelWidth == 1 {
			nextPixel = uint16(pixelBytes[pos])
		} else {
			nextPixel = binary.BigEndian.Uint16(pixelBytes[pos : pos + pixelWidth])
		}

		if pos > 0 && pos % (width * pixelWidth) == 0 {
			componentByte++
		} else if pos % (width * pixelWidth) > 0 {
			if nextPixel > pixel {
				component[componentByte] |= byte(1 << uint(8 - (pos % (width * pixelWidth))))
			}
		}

		pixel = nextPixel
	}

	return
}

func generateFingerprint(path string) (fingerprint Fingerprint, err error) {
	fingerprint.Path = path

	cmd := exec.Command("ffprobe", "-i", path, "-show_entries", "format=duration", "-of", "csv=p=0")
	var horizontalHashComponent, verticalHashComponent hash
	var stdout bytes.Buffer
	var stderr bytes.Buffer

	cmd.Stdout = &stdout
	cmd.Stderr = &stderr

	err = cmd.Run()

	if err != nil {
		err = errors.New(fmt.Sprintf("failed to detect duration: %v", err))

		return
	}

	duration, err := strconv.ParseFloat(strings.TrimSpace(stdout.String()), 64)

	if err != nil {
		return
	}

	interval := duration / 12
	var videoPos float64

	for i := 0; i < 12; i++ {
		cmd = exec.Command("ffmpeg", "-ss", strconv.FormatFloat(videoPos, 'f', -1, 64), "-i", path, "-f", "image2pipe", "-vcodec", "pgm", "-vf", "scale=9x8", "-vframes", "1", "pipe:1", "-hide_banner", "-loglevel", "warning")

		stdout.Reset()
		stderr.Reset()

		cmd.Stdout = &stdout
		cmd.Stderr = &stderr

		err = cmd.Run()

		if err != nil {
			err = errors.New(fmt.Sprintf("failed to get frame sample at %v: %v", videoPos, err))

			return
		}

		horizontalHashComponent, err = generateHashComponent(stdout.Bytes())

		if err != nil {
			return
		}

		cmd = exec.Command("ffmpeg", "-ss", strconv.FormatFloat(videoPos, 'f', -1, 64), "-i", path, "-f", "image2pipe", "-vcodec", "pgm", "-vf", "scale=8x9,transpose=1", "-vframes", "1", "pipe:1", "-hide_banner", "-loglevel", "warning")

		stdout.Reset()
		stderr.Reset()

		cmd.Stdout = &stdout
		cmd.Stderr = &stderr

		err = cmd.Run()

		if err != nil {
			err = errors.New(fmt.Sprintf("failed to get frame sample at %v: %v", videoPos, err))

			return
		}

		verticalHashComponent, err = generateHashComponent(stdout.Bytes())

		if err != nil {
			return
		}

		copy(fingerprint.Fingerprint[i * 16 :], horizontalHashComponent[:])
		copy(fingerprint.Fingerprint[i * 16 + 8 :], verticalHashComponent[:])

		videoPos += interval
	}

	return
}

func generateFingerprints(paths []string) (fingerprints []Fingerprint, err error) {
	var fingerprint Fingerprint

	log.Printf("calculating fingerprints for %v videos", len(paths))

	for pathIndex, path := range paths {
		fingerprint, err = generateFingerprint(path)

		if err == nil {
			fingerprints = append(fingerprints, fingerprint)
		} else {
			log.Printf("%v: failed to calculate fingerprint: %v", path, err)

			err = nil
		}

		log.Printf("calculated fingerprints for %v/%v videos (%v successful)", pathIndex + 1, len(paths), len(fingerprints))
	}

	return
}

func identifySimilarityGroups(fingerprints []Fingerprint) (groups []SimilarityGroup, err error) {
	var processedPairs [][]string
	var alreadyProcessed bool
	var distance uint
	var group SimilarityGroup

	for _, fingerprint := range fingerprints {
		for _, otherFingerprint := range fingerprints {
			if fingerprint.Path == otherFingerprint.Path {
				continue
			}

			alreadyProcessed = false

			for _, processedPair := range processedPairs {
				if processedPair[0] == fingerprint.Path && processedPair[1] == otherFingerprint.Path {
					alreadyProcessed = true

					break
				} else if processedPair[1] == fingerprint.Path && processedPair[0] == otherFingerprint.Path {
					alreadyProcessed = true

					break
				}
			}

			if alreadyProcessed {
				continue
			}

			distance = fingerprint.calculateDistance(otherFingerprint)

			if distance <= SimilarityThreshold {
				if len(group) == 0 {
					group = append(group, fingerprint)
				}

				alreadyProcessed = false

				for _, groupMember := range group {
					if otherFingerprint.Path == groupMember.Path {
						alreadyProcessed = true

						break
					}
				}

				if !alreadyProcessed {
					group = append(group, otherFingerprint)
				}
			}

			processedPairs = append(processedPairs, []string{fingerprint.Path, otherFingerprint.Path})
		}

		if len(group) > 0 {
			groups = append(groups, group)
			group = SimilarityGroup{}
		}
	}

	return
}

func identifySimilarVideos(paths []string) (groups []SimilarityGroup, err error) {
	fingerprints, err := generateFingerprints(paths)

	if err != nil {
		return
	}

	groups, err = identifySimilarityGroups(fingerprints)

	return
}

func getPaths(inPaths []string) (outPaths []string, err error) {
	var info os.FileInfo

	walkFn := func(path string, info os.FileInfo, err error) error {
		if err != nil {
			log.Print(err)

			if info == nil {
				return nil
			} else if info.Mode().IsDir() {
				return filepath.SkipDir
			}

			return nil
		}

		if !info.Mode().IsDir() {
			outPaths = append(outPaths, path)
		}

		return nil
	}

	for _, path := range inPaths {
		info, err = os.Stat(path)

		if err != nil {
			log.Print(err)

			continue
		}

		if info.Mode().IsDir() {
			if err = filepath.Walk(path, walkFn); err != nil {
				return
			}
		} else {
			outPaths = append(outPaths, path)
		}
	}

	return
}

func main() {
	paths, err := getPaths(os.Args[1 :])

	if err != nil {
		log.Fatal(err)
	}

	groups, err := identifySimilarVideos(paths)

	if err != nil {
		log.Fatal(err)
	}

	if len(groups) == 1 {
		log.Printf("%v group identified", len(groups))
	} else {
		log.Printf("%v groups identified", len(groups))
	}

	for groupIndex, group := range groups {
		fmt.Printf("group %v/%v (%v files):\n", groupIndex + 1, len(groups), len(group))

		for fingerprintIndex, fingerprint := range group {
			fmt.Printf("\tfile %v/%v - %v:\n", fingerprintIndex + 1, len(group), fingerprint.Path)
			fmt.Printf("\t\tsimilarity with file 1: %v\n", fingerprint.calculateDistance(group[0]))
			fmt.Printf("\t\tfingerprint: %v\n", hex.EncodeToString(fingerprint.Fingerprint[:]))
		}

		fmt.Println()
	}
}
